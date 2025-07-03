//
//  RNEOSHost.cpp
//  Rayne-EOS
//
//  Copyright 2021 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNEOSHost.h"

#include <RayneConfig.h>

#include "RNEOSWorld.h"

#include "eos_common.h"
#include "eos_p2p.h"
#include "eos_p2p_types.h"
#include "eos_platform_prereqs.h"
#include "eos_sdk.h"

#define MAX_PACKET_SIZE 1000 //Max packet size: 1170, but seems to have issues, so trying 1000

namespace RN
{
	RNDefineMeta(EOSHost, Object)

	EOSHost::EOSHost(RN::String *socketID) :
		_pingTimer(10.0), _status(Disconnected), _clientID(CLIENT_ID_NONE), _socketID(SafeRetain(socketID))
	{
		RN_ASSERT(_socketID, "Socket id needs to be set and needs to be unique per host if there are multiple!");
		RN_ASSERT(_socketID->GetLength() < EOS_P2P_SOCKETID_SOCKETNAME_SIZE, "Socket ID length needs to be 32 or less!");
	}

	EOSHost::~EOSHost()
	{
		SafeRelease(_socketID);
	}

	bool EOSHost::IsPacketInOrder(ProtocolPacketType packetType, EOS_ProductUserId senderID, uint8 packetID, uint8 channel)
	{
		EOSWorld *world = EOSWorld::GetInstance();
		Peer &peer = _peers[senderID];

		//This assumes that less than 127 packets are ever lost at once...
		if(packetType == ProtocolPacketTypeReliableData || peer._receivedIDForChannel[channel] < packetID || (peer._receivedIDForChannel[channel] > 127 && packetID < 127))
		{
			peer._receivedIDForChannel[channel] = packetID;
			return true;
		}

		return false;
	}

	void EOSHost::SendPing(EOS_ProductUserId receiverID, bool isResponse, uint8 responseID)
	{
		Lock();
		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_SocketId socketID = {};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, _socketID->GetUTF8String(), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

		ProtocolPacketHeader packetHeader;
		if(isResponse)
		{
			packetHeader.packetType = ProtocolPacketTypePingResponse;
			packetHeader.packetID = responseID;
			packetHeader.dataLength = 0;
		}
		else
		{
			packetHeader.packetType = ProtocolPacketTypePingRequest;
			packetHeader.packetID = _peers[receiverID]._lastPingID++;
			packetHeader.dataLength = 0;

			_peers[receiverID]._sentPingTime = Clock::now();
		}

		EOS_P2P_SendPacketOptions sendPacketOptions = {};
		sendPacketOptions.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
		sendPacketOptions.Channel = 255;
		sendPacketOptions.LocalUserId = world->GetUserID();
		sendPacketOptions.RemoteUserId = receiverID;
		sendPacketOptions.SocketId = &socketID;
		sendPacketOptions.Reliability = EOS_EPacketReliability::EOS_PR_UnreliableUnordered;
		sendPacketOptions.bAllowDelayedDelivery = false;
		sendPacketOptions.Data = &packetHeader;
		sendPacketOptions.DataLengthBytes = sizeof(packetHeader);
		EOS_P2P_SendPacket(world->GetP2PHandle(), &sendPacketOptions);
		Unlock();
	}

	void EOSHost::SendPacket(Data *data, EOS_ProductUserId receiverID, uint32 channel, bool reliable)
	{
		Lock();
		if(_peers[receiverID]._wantsDisconnect)
		{
			Unlock();
			return; //Don't allow sending more data to users that are about to be disconnected.
		}

		if(_peers.find(receiverID) == _peers.end())
		{
			Unlock();
			RNDebug("Unknown peer " << receiverID);
			return;
		}

		if(_peers[receiverID]._scheduledPackets.find(channel) != _peers[receiverID]._scheduledPackets.end())
		{
			_peers[receiverID]._scheduledPackets.insert(std::pair(channel, std::queue<Packet>()));
		}

		_peers[receiverID]._scheduledPackets[channel].push({receiverID, channel, reliable, data->Retain()});

		Unlock();
	}

	void EOSHost::SendPacket(Data *data, uint8 receiverID, uint32 channel, bool reliable)
	{
		//Only reliable packets can be split up, unreliable packets need to be small enough to fit a single networking packet
		RN_DEBUG_ASSERT(data->GetLength() < MAX_PACKET_SIZE || reliable, "Packet too big!");

		if(!reliable && data->GetLength() >= MAX_PACKET_SIZE) return; //Don't send if unreliable packet is too big. Since it is unreliable, not sending it is acceptable.

		Lock();
		if(_idMap.find(receiverID) == _idMap.end())
		{
			Unlock();
			RNDebug("Unknown receiver ID " << receiverID);
			return;
		}
		EOS_ProductUserId internalReceiverID = _idMap[receiverID];
		SendPacket(data, internalReceiverID, channel, reliable);
		Unlock();
	}

	void EOSHost::BroadcastPacket(Data *data, uint32 channel, bool reliable, uint8 excludeClientID)
	{
		for(auto peer : _peers)
		{
			if(excludeClientID != CLIENT_ID_NONE && peer.second.clientID == excludeClientID) continue;
			SendPacket(data, peer.first, channel, reliable);
		}
	}

	void EOSHost::ReceivedPacketInternal(uint8 *rawData, uint32 bytesWritten, EOS_ProductUserId senderUserID, uint8 channel)
	{
		Lock();
		if(channel == 255) //This is a ping!
		{
			uint16 dataIndex = 0;
			while(dataIndex < bytesWritten)
			{
				ProtocolPacketHeader packetHeader;
				packetHeader.packetType = static_cast<ProtocolPacketType>(rawData[dataIndex + 0]);
				packetHeader.packetID = rawData[dataIndex + 1];
				packetHeader.dataLength = rawData[dataIndex + 2] | (rawData[dataIndex + 3] << 8); //These are pings, so this should always be 0!?

				if(packetHeader.packetType == ProtocolPacketTypePingRequest)
				{
					SendPing(senderUserID, true, packetHeader.packetID);
				}
				else if(packetHeader.packetType == ProtocolPacketTypePingResponse)
				{
					if(_peers[senderUserID]._lastPingID - 1 == packetHeader.packetID)
					{
						Clock::time_point receivedPingTime = Clock::now();
						auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(receivedPingTime - _peers[senderUserID]._sentPingTime).count();
						double timeElapsed = milliseconds / 1000.0;

						//RNDebug("Ping time for " << id << ": " << timeElapsed);

						_peers[senderUserID].smoothedRoundtripTime = _peers[senderUserID].smoothedRoundtripTime * 0.75 + timeElapsed * 0.25;
					}
					else
					{
						RNDebug("Missed a ping!");
					}
				}

				dataIndex += packetHeader.dataLength + 4;
			}
		}
		Unlock();
	}

	void EOSHost::Update(float delta)
	{
		Lock();
		EOSWorld *world = EOSWorld::GetInstance();

		_pingTimer += delta;
		if(_pingTimer > 5.0)
		{
			_pingTimer = 0.0f;
			for(auto &pair : _peers)
			{
				SendPing(pair.first, false, 0);
			}
		}
		
		EOS_P2P_SocketId socketID = {};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, _socketID->GetUTF8String(), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

		//size_t scheduled_count = 0;
		//size_t sent_count = 0;
		for(auto &peer : _peers)
		{
			for(auto &pair : peer.second._scheduledPackets)
			{
				auto &scheduledPackets = pair.second;
				while(scheduledPackets.size() > 0)
				{
					Data *data = new Data();
					bool isReliable = false;

					if(scheduledPackets.front().data->GetLength() + 4 >= MAX_PACKET_SIZE) //If it does not fit into a single networking packet
					{
						RN_DEBUG_ASSERT(scheduledPackets.front().isReliable, "Large packets (>= 1000 byte) need to be reliable!");

						uint8 packetID = peer.second._packetIDForChannel[pair.first]++;
						isReliable = true;

						Data *packetData = scheduledPackets.front().data;
						scheduledPackets.pop();

						uint16 totalParts = std::ceil(packetData->GetLength() / static_cast<float>(MAX_PACKET_SIZE - 6)); //Get total number of parts to split the data up to
						uint32 dataOffset = 0;
						for(uint16 currentPart = 0; currentPart < totalParts; currentPart += 1)
						{
							uint8 headerData[2];
							headerData[0] = ProtocolPacketTypeReliableDataMultipart;
							headerData[1] = packetID;

							uint16 multiPartHeaderData[2];
							multiPartHeaderData[0] = currentPart;
							multiPartHeaderData[1] = totalParts;

							RNDebug("Sending multipart data (" << packetID << "), part " << currentPart << " of " << totalParts);

							data->Append(headerData, 2);
							data->Append(multiPartHeaderData, 4);

							uint32 dataLength = std::min(static_cast<uint32>(MAX_PACKET_SIZE - 6), static_cast<uint32>(packetData->GetLength() - dataOffset));
							data->Append(packetData->GetDataInRange(Range(dataOffset, dataLength)));
							dataOffset += dataLength;

							EOS_P2P_SendPacketOptions sendPacketOptions = {};
							sendPacketOptions.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
							sendPacketOptions.Channel = pair.first;
							sendPacketOptions.LocalUserId = world->GetUserID();
							sendPacketOptions.RemoteUserId = peer.second.internalID;
							sendPacketOptions.SocketId = &socketID;
							sendPacketOptions.Reliability = EOS_EPacketReliability::EOS_PR_ReliableOrdered;
							sendPacketOptions.bAllowDelayedDelivery = false;
							sendPacketOptions.Data = data->GetBytes();
							sendPacketOptions.DataLengthBytes = data->GetLength();
							EOS_P2P_SendPacket(world->GetP2PHandle(), &sendPacketOptions);
							data->Release(); //Should keep data around and just clear it somehow to not reallocate all the time
							data = new Data();
						}

						data->Release();
						packetData->Release();
					}
					else
					{
						//Combine as many packets into one as possible to improve performance
						while(scheduledPackets.size() > 0 && data->GetLength() + scheduledPackets.front().data->GetLength() + 4 < MAX_PACKET_SIZE)
						{
							uint8 headerData[2];
							headerData[1] = peer.second._packetIDForChannel[pair.first]++;

							ProtocolPacketType packetType = ProtocolPacketTypeData;
							if(scheduledPackets.front().isReliable)
							{
								isReliable = true;
								packetType = ProtocolPacketTypeReliableData;
							}
							headerData[0] = packetType;

							data->Append(headerData, 2);
							uint16 dataLength = scheduledPackets.front().data->GetLength();
							data->Append(&dataLength, 2); //Data length is actually part of the header, but much easier to just set here
							data->Append(scheduledPackets.front().data);

							scheduledPackets.front().data->Release();
							scheduledPackets.pop();

							//scheduled_count += 1;
						}

						EOS_P2P_SendPacketOptions sendPacketOptions = {};
						sendPacketOptions.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
						sendPacketOptions.Channel = pair.first;
						sendPacketOptions.LocalUserId = world->GetUserID();
						sendPacketOptions.RemoteUserId = peer.second.internalID;
						sendPacketOptions.SocketId = &socketID;
						sendPacketOptions.Reliability = isReliable ? EOS_EPacketReliability::EOS_PR_ReliableOrdered : EOS_EPacketReliability::EOS_PR_UnreliableUnordered;
						sendPacketOptions.bAllowDelayedDelivery = false;
						sendPacketOptions.Data = data->GetBytes();
						sendPacketOptions.DataLengthBytes = data->GetLength();
						EOS_P2P_SendPacket(world->GetP2PHandle(), &sendPacketOptions);
						data->Release(); //Should keep data around and just clear it somehow to not reallocate all the time

						//sent_count += 1;
					}
				}
			}
		}

		Unlock();

		//if(scheduled_count + sent_count > 0) RNDebug("Did send " << scheduled_count << " scheduled packets as " << sent_count << " packets to " << _peers.size() << " peers.");
	}

	EOSHost::Peer EOSHost::CreatePeer(uint8 clientID, EOS_ProductUserId internalID)
	{
		Peer peer;
		peer.clientID = clientID;
		peer.internalID = internalID;
		peer.smoothedRoundtripTime = 0.05;
		peer._lastPingID = 0;
		peer._wantsDisconnect = false;
		peer._disconnectDelay = 0.0f;

		for(int i = 0; i < 254; i++)
		{
			peer._packetIDForChannel[i] = 0;
			peer._receivedIDForChannel[i] = 255;
		}

		return peer;
	}

	uint8 EOSHost::GetUserIDForInternalID(EOS_ProductUserId internalID)
	{
		auto it = _peers.find(internalID);
		if(it != _peers.end()) return it->second.clientID;
		return CLIENT_ID_NONE;
	}

	double EOSHost::GetLastRoundtripTime(uint8 peerID)
	{
		return _peers[_idMap[peerID]].smoothedRoundtripTime;
	}

	bool EOSHost::HasClient(RN::uint8 clientID)
	{
		if(_idMap.find(clientID) == _idMap.end()) return false;
		if(_peers.find(_idMap.at(clientID)) == _peers.end()) return false;
		return true;
	}

	bool EOSHost::HasClient(const RN::String *eosUserIDString)
	{
		if(!eosUserIDString) return false;

		EOS_ProductUserId eosID = EOSWorld::GetInstance()->GetUserIDFromString(eosUserIDString);
		if(_peers.find(eosID) == _peers.end()) return false;
		return true;
	}
} // namespace RN
