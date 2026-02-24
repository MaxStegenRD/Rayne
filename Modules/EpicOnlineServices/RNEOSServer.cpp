//
//  RNEOSServer.cpp
//  Rayne-EOS
//
//  Copyright 2021 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include "RNEOSServer.h"
#include "RNEOSWorld.h"

#include "eos_common.h"
#include "eos_p2p.h"
#include "eos_p2p_types.h"
#include "eos_platform_prereqs.h"
#include "eos_sdk.h"

namespace RN
{
	RNDefineMeta(EOSServer, EOSHost)

	EOSServer::EOSServer(uint8 maxConnections) :
		EOSHost(RNCSTR("FuckYeah")), _maxConnections(maxConnections)
	{
		Lock();
		_status = Server;

		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_SocketId socketID = {};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, _socketID->GetUTF8String(), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

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

	EOSServer::~EOSServer()
	{
		EOSWorld *world = EOSWorld::GetInstance();
		if(world && world->GetP2PHandle())
		{
			if(_connectionClosedNotificationID != EOS_INVALID_NOTIFICATIONID) EOS_P2P_RemoveNotifyPeerConnectionClosed(world->GetP2PHandle(), _connectionClosedNotificationID);
			if(_connectionRequestNotificationID != EOS_INVALID_NOTIFICATIONID) EOS_P2P_RemoveNotifyPeerConnectionRequest(world->GetP2PHandle(), _connectionRequestNotificationID);
		}
	}

	uint8 EOSServer::GetUserID()
	{
		for(uint8 freeID = 1; freeID < std::min(_maxConnections, CLIENT_ID_RESERVED); freeID++)
		{
			if(!_activeUserIDs.count(freeID))
			{
				_activeUserIDs.insert(freeID);
				return freeID;
			}
		}

		return CLIENT_ID_NONE;
	}

	void EOSServer::ReleaseUserID(uint8 userID)
	{
		_activeUserIDs.erase(userID);
	}

	void EOSServer::ReceivedPacketInternal(uint8 *rawData, uint32 bytesWritten, EOS_ProductUserId senderUserID, uint8 channel)
	{
		EOSHost::ReceivedPacketInternal(rawData, bytesWritten, senderUserID, channel);
		if(channel == 255) return; //This is a ping, handled by the Host class
		
		Lock();
		
		Peer &peer = _peers[senderUserID];
		uint8 senderID = peer.clientID;

		if(static_cast<ProtocolPacketType>(rawData[0]) == ProtocolPacketTypeReliableDataMultipart)
		{
			//If this is multipart data, wait for all parts before passing them on
			ProtocolPacketHeaderMultipart packetHeader;
			packetHeader.packetType = static_cast<ProtocolPacketType>(rawData[0]);
			packetHeader.packetID = rawData[1];
			packetHeader.dataPart = rawData[2] | rawData[3] << 8;
			packetHeader.totalDataParts = rawData[4] | rawData[5] << 8;

			//RNDebug("Received multipart data (" << packetHeader.packetID <<  "), part " << packetHeader.dataPart << " of " << packetHeader.totalDataParts);

			if(peer._multipartPacketTotalParts.count(channel) == 0)
			{
				//Received new multipart data!

				if(packetHeader.dataPart != 0)
				{
					//Received new multipart data, but it's missing previous parts!?
					//TODO: Consider disconnecting user? For now just skip the data. But this case means that something is seriously wrong.
					//Nothing to clean up here, as no data exists at this point

					RNDebug("Received multipart data but it's bad!");
					Unlock();
					return;
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

					RNDebug("Received multipart data but it's bad!");

					peer._multipartPacketTotalParts.erase(channel);
					peer._multipartPacketCurrentPart.erase(channel);
					peer._multipartPacketID.erase(channel);
					peer._multipartPacketData[channel]->Release();
					peer._multipartPacketData.erase(channel);

					Unlock();
					return;
				}

				peer._multipartPacketCurrentPart[channel] = packetHeader.dataPart;
			}

			//Append data from the packet without protocol header
			peer._multipartPacketData[channel]->Append(&rawData[6], bytesWritten - 6);

			if(packetHeader.dataPart + 1 >= packetHeader.totalDataParts)
			{
				//RNDebug("Received full multipart data");

				Unlock();
				ReceivedPacket(peer._multipartPacketData[channel], senderID, channel);
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

					Unlock();
					return;
				}
				else
				{
					//Got out of order unreliable data while still waiting for multipart data. Just ignore and wait for remaining multipart data.
					Unlock();
					return;
				}
			}

			size_t dataIndex = 0;
			while(dataIndex < bytesWritten)
			{
				ProtocolPacketHeader packetHeader;
				packetHeader.packetType = static_cast<ProtocolPacketType>(rawData[dataIndex + 0]);
				packetHeader.packetID = rawData[dataIndex + 1];
				packetHeader.dataLength = rawData[dataIndex + 2] | (rawData[dataIndex + 3] << 8);

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

				if(!IsPacketInOrder(packetHeader.packetType, senderUserID, packetHeader.packetID, channel) || _peers[senderUserID]._wantsDisconnect) //Don't process any more data from a user that is about to be disconnected
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
		
		Unlock();
	}

	void EOSServer::Update(float delta)
	{
		EOSHost::Update(delta); //This sends regular pings and handles sending of scheduled packets

		Lock();
		
		std::vector<uint8> peersToDisconnect;
		for(auto &pair : _peers)
		{
			if(pair.second._wantsDisconnect)
			{
				pair.second._disconnectDelay -= delta;
				if(pair.second._disconnectDelay < 0.0f)
				{
					peersToDisconnect.push_back(pair.second.clientID);
				}
			}
		}

		for(uint8 clientID : peersToDisconnect)
		{
			DisconnectUser(clientID);
		}

		Unlock();
	}

	size_t EOSServer::GetNumberOfConnectedUsers() const
	{
		return _activeUserIDs.size();
	}

	void EOSServer::DisconnectUserDelayed(uint8 userID, uint16 data, float delay)
	{
		Lock();
		EOS_ProductUserId internalID = _idMap[userID];
		_peers[internalID]._disconnectDelay = delay;
		_peers[internalID]._wantsDisconnect = true;
		Unlock();
	}

	void EOSServer::DisconnectUser(uint8 userID)
	{
		Lock();
		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_SocketId socketID = {};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, _socketID->GetUTF8String(), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

		EOS_P2P_CloseConnectionOptions options;
		options.ApiVersion = EOS_P2P_CLOSECONNECTION_API_LATEST;
		options.LocalUserId = world->GetUserID();
		options.RemoteUserId = _idMap[userID];
		options.SocketId = &socketID;

		EOS_P2P_CloseConnection(world->GetP2PHandle(), &options);
		Unlock();
	}

	void EOSServer::Disconnect()
	{
		Lock();
		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_SocketId socketID = {};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, _socketID->GetUTF8String(), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

		EOS_P2P_CloseConnectionsOptions options;
		options.ApiVersion = EOS_P2P_CLOSECONNECTION_API_LATEST;
		options.LocalUserId = world->GetUserID();
		options.SocketId = &socketID;

		EOS_P2P_CloseConnections(world->GetP2PHandle(), &options);
		Unlock();
	}

	void EOSServer::DisconnectClient(EOS_ProductUserId productUserId)
	{
		if(productUserId == EOSWorld::GetInstance()->GetUserID())
		{
			Disconnect();
		}
		else if(_peers.find(productUserId) != _peers.end())
		{
			DisconnectUser(_peers[productUserId].clientID);
		}
		else
		{
			RNWarning("Trying to disconnect unknown peer " << productUserId);
		}
	}

	void EOSServer::OnConnectionRequestCallback(const EOS_P2P_OnIncomingConnectionRequestInfo *Data)
	{
		EOSServer *server = static_cast<EOSServer *>(Data->ClientData);
		EOSWorld *world = EOSWorld::GetInstance();

		EOS_P2P_SocketId socketID = {};
		socketID.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
		strncpy(socketID.SocketName, server->GetSocketID()->GetUTF8String(), EOS_P2P_SOCKETID_SOCKETNAME_SIZE);

		EOS_P2P_AcceptConnectionOptions connectionOptions;
		connectionOptions.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
		connectionOptions.SocketId = &socketID;
		connectionOptions.LocalUserId = world->GetUserID();
		connectionOptions.RemoteUserId = Data->RemoteUserId;
		EOS_P2P_AcceptConnection(world->GetP2PHandle(), &connectionOptions);

		RNDebug("A new client connected");
		const Peer &peer = server->CreatePeer(server->GetUserID(), Data->RemoteUserId);
		server->_peers.insert(std::pair(peer.internalID, peer));
		server->_idMap[peer.clientID] = peer.internalID;

		ProtocolPacketHeader packetHeader;
		packetHeader.packetType = ProtocolPacketTypeConnectResponse;
		packetHeader.packetID = 0;
		packetHeader.dataLength = 4;

		RN::Data *packetData = new RN::Data();
		packetData->Append(&packetHeader, 4);
		uint8 serverUserID = server->_clientID;
		packetData->Append(&serverUserID, sizeof(uint8));
		packetData->Append(&peer.clientID, sizeof(uint8));

		EOS_P2P_SendPacketOptions connectConfirmOptions = {};
		connectConfirmOptions.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
		connectConfirmOptions.SocketId = &socketID;
		connectConfirmOptions.LocalUserId = world->GetUserID();
		connectConfirmOptions.RemoteUserId = Data->RemoteUserId;
		connectConfirmOptions.Channel = 0;
		connectConfirmOptions.Reliability = EOS_EPacketReliability::EOS_PR_ReliableOrdered;
		connectConfirmOptions.bAllowDelayedDelivery = true;
		connectConfirmOptions.DataLengthBytes = packetData->GetLength();
		connectConfirmOptions.Data = packetData->GetBytes();

		EOS_P2P_SendPacket(world->GetP2PHandle(), &connectConfirmOptions);

		server->HandleDidConnect(peer.clientID);
	}

	void EOSServer::OnConnectionClosedCallback(const EOS_P2P_OnRemoteConnectionClosedInfo *Data)
	{
		EOSServer *server = static_cast<EOSServer *>(Data->ClientData);

		uint8 id = server->_peers[Data->RemoteUserId].clientID;

		RNDebug("Client disconnected: " << id);
		server->_idMap.erase(id);
		server->_peers.erase(Data->RemoteUserId);
		server->ReleaseUserID(id);
		server->HandleDidDisconnect(id, static_cast<uint16>(Data->Reason));
	}
} // namespace RN
