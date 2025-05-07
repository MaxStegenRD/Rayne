//
//  RNEOSP2PClient.cpp
//  Rayne-EOS
//
//  Copyright 2025 by Guacam. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNEOSP2PClient.h"
#include "RNEOSWorld.h"

#include "eos_common.h"
#include "eos_p2p.h"
#include "eos_p2p_types.h"
#include "eos_platform_prereqs.h"

namespace RN
{
	RNDefineMeta(EOSP2PClient, EOSHost)

	EOSP2PClient::EOSP2PClient(bool isHost, uint16 maxConnections) :
		_isHost(isHost), _maxConnections(maxConnections)
	{
		Lock();
		_userID = _isHost ? 0 : 0xFFFF;
		_status = _isHost ? Server : Disconnected;

		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_SocketId socketID = {};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, "FuckYeah", EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

		EOS_P2P_AddNotifyPeerConnectionRequestOptions connectListenerOptions;
		connectListenerOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
		connectListenerOptions.LocalUserId = world->GetUserID();
		connectListenerOptions.SocketId = &socketID;
		_connectionRequestNotificationID = EOS_P2P_AddNotifyPeerConnectionRequest(world->GetP2PHandle(), &connectListenerOptions, this, OnConnectionRequestCallback);

		EOS_P2P_AddNotifyPeerConnectionClosedOptions disconnectListenerOptions;
		disconnectListenerOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONCLOSED_API_LATEST;
		disconnectListenerOptions.LocalUserId = world->GetUserID();
		disconnectListenerOptions.SocketId = &socketID;
		_connectionClosedNotificationID = EOS_P2P_AddNotifyPeerConnectionClosed(world->GetP2PHandle(), &disconnectListenerOptions, this, OnConnectionClosedCallback);

		Unlock();
	}

	EOSP2PClient::~EOSP2PClient()
	{
		EOSWorld *world = EOSWorld::GetInstance();
		EOS_P2P_RemoveNotifyPeerConnectionRequest(world->GetP2PHandle(), _connectionRequestNotificationID);
		EOS_P2P_RemoveNotifyPeerConnectionClosed(world->GetP2PHandle(), _connectionClosedNotificationID);
	}

	void EOSP2PClient::Connect(EOS_ProductUserId remoteProductUserID)
	{
		Lock();
		RN_ASSERT(_status == Disconnected, "Cannot connect in current status." + _status);

		_status = Connecting;

		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_SocketId socketID = {};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, "FuckYeah", EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

		ProtocolPacketHeader packetHeader;
		packetHeader.packetType = ProtocolPacketTypeConnectRequest;
		packetHeader.packetID = 0;
		packetHeader.dataLength = 0;

		EOS_P2P_SendPacketOptions connectionOptions = {};
		connectionOptions.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
		connectionOptions.SocketId = &socketID;
		connectionOptions.LocalUserId = world->GetUserID();
		connectionOptions.RemoteUserId = remoteProductUserID;
		connectionOptions.Channel = 0;
		connectionOptions.Reliability = EOS_EPacketReliability::EOS_PR_ReliableOrdered;
		connectionOptions.bAllowDelayedDelivery = true;
		connectionOptions.DataLengthBytes = sizeof(packetHeader);
		connectionOptions.Data = &packetHeader;

		EOS_EResult result = EOS_P2P_SendPacket(world->GetP2PHandle(), &connectionOptions);

		if(result != EOS_EResult::EOS_Success) //TODO only do this when connecting to host failed
		{
			RNDebug("Failed to connect to " << remoteProductUserID);
			ForceDisconnect(0);
			Unlock();
			return;
		}

		Unlock();
	}

	void EOSP2PClient::Disconnect()
	{
		Lock();
		if(_status == Disconnected || _status == Disconnecting)
		{
			Unlock();
			return;
		}

		if(_status == Connecting)
		{
			Unlock();
			ForceDisconnect(0);
			return;
		}

		_status = Disconnecting;

		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_SocketId socketID = {0};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, "FuckYeah", EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

		EOS_P2P_CloseConnectionsOptions options = {0};
		options.ApiVersion = EOS_P2P_CLOSECONNECTION_API_LATEST;
		options.LocalUserId = world->GetUserID();
		options.SocketId = &socketID;

		EOS_P2P_CloseConnections(world->GetP2PHandle(), &options);

		Unlock();
	}
	
	void EOSP2PClient::DisconnectUser(uint16 userID, uint16 data)
	{
		Lock();
		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_SocketId socketID = {};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, "FuckYeah", EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

		EOS_P2P_CloseConnectionOptions options;
		options.ApiVersion = EOS_P2P_CLOSECONNECTION_API_LATEST;
		options.LocalUserId = world->GetUserID();
		options.RemoteUserId = _idMap[userID];
		options.SocketId = &socketID;

		EOS_P2P_CloseConnection(world->GetP2PHandle(), &options);
		Unlock();
	}
	
	void EOSP2PClient::DisconnectUserDelayed(uint16 userID, float delay)
	{
		Lock();
		EOS_ProductUserId internalID = _idMap[userID];
		_peers[internalID]._disconnectDelay = delay;
		_peers[internalID]._wantsDisconnect = true;
		Unlock();
	}
	
	void EOSP2PClient::DisconnectAll()
	{
		Lock();
		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_SocketId socketID = {};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, "FuckYeah", EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

		EOS_P2P_CloseConnectionsOptions options;
		options.ApiVersion = EOS_P2P_CLOSECONNECTION_API_LATEST;
		options.LocalUserId = world->GetUserID();
		options.SocketId = &socketID;

		EOS_P2P_CloseConnections(world->GetP2PHandle(), &options);
		Unlock();
	}

	void EOSP2PClient::ForceDisconnect(uint16 reason)
	{
		Lock();
		_status = Disconnected;
		_peers.clear();
		_idMap.clear();
		Unlock();

		RNDebug("Disconnected!");
		HandleDidDisconnect(0, reason);
	}

	uint16 EOSP2PClient::GetUnusedUserID() const
	{
		for(uint16 freeID = 1; freeID < _maxConnections; freeID++)
		{
			if(_idMap.find(freeID) == _idMap.end() && freeID != _userID)
			{
				return freeID;
			}
		}

		return -1;
	}

	void EOSP2PClient::Update(float delta)
	{
		EOSHost::Update(delta); //Needs to go first as it picks out some packets! TODO: This also handles sending new packets, would reduce some latency if this was done at the end of this method

		Lock();
		if(_status == Disconnected)
		{
			Unlock();
			return;
		}

		EOSWorld *world = EOSWorld::GetInstance();

		uint32 nextPacketSize = 0;
		EOS_P2P_GetNextReceivedPacketSizeOptions nextPacketSizeOptions = {};
		nextPacketSizeOptions.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
		nextPacketSizeOptions.LocalUserId = world->GetUserID();
		while(EOS_P2P_GetNextReceivedPacketSize(world->GetP2PHandle(), &nextPacketSizeOptions, &nextPacketSize) == EOS_EResult::EOS_Success)
		{
			if(nextPacketSize < sizeof(ProtocolPacketHeader))
			{
				RNDebug("Packet too small, this is not supposed to ever happen...");
				continue;
			}

			EOS_P2P_ReceivePacketOptions receiveOptions = {};
			receiveOptions.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
			receiveOptions.LocalUserId = world->GetUserID();
			receiveOptions.MaxDataSizeBytes = nextPacketSize;

			EOS_ProductUserId senderUserID;
			EOS_P2P_SocketId socketID;
			uint8 channel = 0;
			uint32 bytesWritten = 0;

			uint8 *rawData = new uint8[nextPacketSize];

			if(EOS_P2P_ReceivePacket(world->GetP2PHandle(), &receiveOptions, &senderUserID, &socketID, &channel, rawData, &bytesWritten) != EOS_EResult::EOS_Success)
			{
				RNDebug("Failed receiving Data");
				break;
			}

			if(_peers.find(senderUserID) == _peers.end())
			{
				//Create peer with unknow user ID. Will be set via connection response
				_peers.insert(std::pair(senderUserID, CreatePeer(0xFFFF, senderUserID)));
			}
			uint16 senderID = _peers[senderUserID].userID;
			Peer &peer = _peers[senderUserID];

			if(static_cast<ProtocolPacketType>(rawData[0]) == ProtocolPacketTypeReliableDataMultipart)
			{
				//If this is multipart data, wait for all parts before passing them on
				ProtocolPacketHeaderMultipart packetHeader;
				packetHeader.packetType = static_cast<ProtocolPacketType>(rawData[0]);
				packetHeader.packetID = rawData[1];
				packetHeader.dataPart = rawData[2] | (rawData[3] << 8);
				packetHeader.totalDataParts = rawData[4] | (rawData[5] << 8);

				//RNDebug("Received multipart data (" << packetHeader.packetID <<  "), part " << packetHeader.dataPart << " of " << packetHeader.totalDataParts);

				if(peer._multipartPacketTotalParts.count(channel) == 0)
				{
					//Received new multipart data!

					if(packetHeader.dataPart != 0)
					{
						//Received new multipart data, but it's missing previous parts!?
						//TODO: Consider disconnecting user? For now just skip the data. But this case means that something is seriously wrong.
						//Nothing to clean up here as the data does not exist yet

						RNDebug("Received multipart data but it's missing previous parts!");

						continue;
					}

					peer._multipartPacketTotalParts[channel] = packetHeader.totalDataParts;
					peer._multipartPacketCurrentPart[channel] = packetHeader.dataPart;
					peer._multipartPacketID[channel] = packetHeader.packetID;
					peer._multipartPacketData[channel] = new Data();
				}
				else
				{
					//Received another part of multipart data!

					if(peer._multipartPacketCurrentPart[channel] + 1 != packetHeader.dataPart || peer._multipartPacketTotalParts[channel] != packetHeader.totalDataParts || peer._multipartPacketID[channel] != packetHeader.packetID)
					{
						//Received multipart data, but found some inconsistency
						//TODO: Consider disconnecting user? For now just skip the data. But this case means that something is seriously wrong.

						RNDebug("Received multipart data but it's missing parts!");

						peer._multipartPacketTotalParts.erase(channel);
						peer._multipartPacketCurrentPart.erase(channel);
						peer._multipartPacketID.erase(channel);
						peer._multipartPacketData[channel]->Release();
						peer._multipartPacketData.erase(channel);

						continue;
					}

					peer._multipartPacketCurrentPart[channel] = packetHeader.dataPart;
				}

				//Append data from the packet without protocol header
				peer._multipartPacketData[channel]->Append(&rawData[6], bytesWritten - 6);

				if(packetHeader.dataPart + 1 >= packetHeader.totalDataParts)
				{
					//RNDebug("Received full multipart data");
					Unlock();
					ReceivedPacket(peer._multipartPacketData[channel], 0, channel);
					Lock();

					peer._multipartPacketTotalParts.erase(channel);
					peer._multipartPacketCurrentPart.erase(channel);
					peer._multipartPacketID.erase(channel);
					peer._multipartPacketData[channel]->Release();
					peer._multipartPacketData.erase(channel);
				}
			}
			else
			{
				if(peer._multipartPacketTotalParts.count(channel) != 0)
				{
					if(static_cast<ProtocolPacketType>(rawData[0]) == ProtocolPacketTypeReliableData)
					{
						//Got non-multipart reliable data on a channel that got multipart data before that is still incomplete.
						//TODO: Consider disconnecting user? For now just skip the data. But this case means that something is seriously wrong.

						RNDebug("Received multipart data but it's incomplete!");

						peer._multipartPacketTotalParts.erase(channel);
						peer._multipartPacketCurrentPart.erase(channel);
						peer._multipartPacketID.erase(channel);
						peer._multipartPacketData[channel]->Release();
						peer._multipartPacketData.erase(channel);

						continue;
					}
					else
					{
						//Got out of order unreliable data while still waiting for multipart data. Just ignore and wait for remaining multipart data.
						continue;
					}
				}

				//All data fits into one packet, though multiple internal packets maybe encoded in a single networking packet and it needs to be unpacked
				uint16 dataIndex = 0;
				while(dataIndex < bytesWritten)
				{
					ProtocolPacketHeader packetHeader;
					packetHeader.packetType = static_cast<ProtocolPacketType>(rawData[dataIndex + 0]);
					packetHeader.packetID = rawData[dataIndex + 1];
					packetHeader.dataLength = rawData[dataIndex + 2] | rawData[dataIndex + 3] << 8;

					if(packetHeader.packetType == ProtocolPacketTypeConnectRequest)
					{
						if(packetHeader.packetID == 0)
						{
							//This is handled in OnConnectionRequestCallback
							//TODO: Maybe move some of it here instead to not require delayed delivery for the response
						}
						else
						{
							RNDebug("Malformed connect request");
						}
						dataIndex += packetHeader.dataLength + 4;
						continue;
					}

					if(packetHeader.packetType == ProtocolPacketTypeConnectResponse)
					{
						if(packetHeader.packetID == 0 && packetHeader.dataLength <= 4)
						{
							RNDebug("Connected to " << senderUserID);
							dataIndex += 4;
							uint16 remoteUserID = rawData[dataIndex] | rawData[dataIndex + 1] << 8;

							if(packetHeader.dataLength > 2)
							{
								uint16 ownUserID = rawData[dataIndex + 2] | rawData[dataIndex + 3] << 8;
								if(ownUserID != 0xFFFF) _userID = ownUserID;
								RNDebug("Was assigned user ID " << ownUserID);
								_status = Connected;
							}

							//Assign user ID
							_peers[senderUserID].userID = remoteUserID;
							_idMap[remoteUserID] = senderUserID;

							Unlock();
							HandleDidConnect(remoteUserID);
							Lock();
						}
						else
						{
							RNDebug("Malformed connect response");
						}
						continue;
					}

					if(!IsPacketInOrder(packetHeader.packetType, senderUserID, packetHeader.packetID, channel))
					{
						dataIndex += packetHeader.dataLength + 4;
						continue;
					}

					//Get data object from the packet without protocol header
					Data *data = Data::WithBytes(&rawData[dataIndex + 4], packetHeader.dataLength);
					dataIndex += packetHeader.dataLength + 4;

					Unlock();
					ReceivedPacket(data, senderID, channel);
					Lock();
				}
			}

			delete[] rawData;
		}

		std::vector<uint16> peersToDisconnect;
		for(auto &pair : _peers)
		{
			if(pair.second._wantsDisconnect)
			{
				pair.second._disconnectDelay -= delta;
				if(pair.second._disconnectDelay < 0.0f)
				{
					peersToDisconnect.push_back(pair.second.userID);
				}
			}
		}

		for(uint16 clientID : peersToDisconnect)
		{
			DisconnectUser(clientID, 0);
		}

		Unlock();
	}

	void EOSP2PClient::OnConnectionRequestCallback(const EOS_P2P_OnIncomingConnectionRequestInfo *Data)
	{
		EOSP2PClient *client = static_cast<EOSP2PClient *>(Data->ClientData);
		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_AcceptConnectionOptions acceptOptions;
		acceptOptions.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
		acceptOptions.LocalUserId = Data->LocalUserId;
		acceptOptions.RemoteUserId = Data->RemoteUserId;
		acceptOptions.SocketId = Data->SocketId;

		EOS_P2P_AcceptConnection(EOSWorld::GetInstance()->GetP2PHandle(), &acceptOptions);

		RNDebug("Remote client connected");

		ProtocolPacketHeader packetHeader;
		packetHeader.packetType = ProtocolPacketTypeConnectResponse;
		packetHeader.packetID = 0;

		RN::Data *packetData = new RN::Data();
		uint16 localUserID = client->_userID;

		//If hosting the session, assign a new client user id, else ask for user id by sending connection request
		if(client->_isHost)
		{
			const Peer &peer = client->CreatePeer(client->GetUnusedUserID(), Data->RemoteUserId);
			client->_peers.insert(std::pair(Data->RemoteUserId, peer));
			client->_idMap[peer.userID] = Data->RemoteUserId;
			packetHeader.dataLength = 4;
			packetData->Append(&packetHeader, 4);
			packetData->Append(&localUserID, sizeof(uint16));
			packetData->Append(&peer.userID, sizeof(uint16));
			client->HandleDidConnect(peer.userID);
		}
		else
		{
			const Peer &peer = client->CreatePeer(0xFFFF, Data->RemoteUserId);
			client->_peers.insert(std::pair(Data->RemoteUserId, peer));
			packetHeader.dataLength = 2;
			packetData->Append(&packetHeader, 4);
			packetData->Append(&localUserID, sizeof(uint16));
		}

		EOS_P2P_SendPacketOptions connectConfirmOptions = {};
		connectConfirmOptions.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
		connectConfirmOptions.SocketId = Data->SocketId;
		connectConfirmOptions.LocalUserId = world->GetUserID();
		connectConfirmOptions.RemoteUserId = Data->RemoteUserId;
		connectConfirmOptions.Channel = 0;
		connectConfirmOptions.Reliability = EOS_EPacketReliability::EOS_PR_ReliableOrdered;
		connectConfirmOptions.bAllowDelayedDelivery = true;
		connectConfirmOptions.DataLengthBytes = packetData->GetLength();
		connectConfirmOptions.Data = packetData->GetBytes();

		EOS_P2P_SendPacket(world->GetP2PHandle(), &connectConfirmOptions);

		if(!client->_isHost)
		{
			bool isClientUserIDAssigned = client->_peers[Data->RemoteUserId].userID != 0xFFFF;
			if(!isClientUserIDAssigned) client->Connect(Data->RemoteUserId); //Asking for user id
		}
	}

	void EOSP2PClient::OnConnectionClosedCallback(const EOS_P2P_OnRemoteConnectionClosedInfo *Data)
	{
		EOSP2PClient *client = static_cast<EOSP2PClient *>(Data->ClientData);

		if(client->_isHost)
		{
			uint16 id = client->_peers[Data->RemoteUserId].userID;
			RNDebug("Client disconnected: " << id);
			client->_idMap.erase(id);
			client->_peers.erase(Data->RemoteUserId);
			client->HandleDidDisconnect(id, static_cast<uint16>(Data->Reason));
		}
		else
		{
			RNDebug("Disconnected from Server");
			client->ForceDisconnect(static_cast<uint16>(Data->Reason));
		}
	}
} // namespace RN
