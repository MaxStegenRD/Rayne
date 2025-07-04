//
//  RNEOSP2PClient.h
//  Rayne-EOS
//
//  Copyright 2025 by Guacam. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#ifndef __RAYNE_EOSP2PCLIENT_H_
#define __RAYNE_EOSP2PCLIENT_H_

#include "RNEOSHost.h"

typedef struct _tagEOS_P2P_OnIncomingConnectionRequestInfo EOS_P2P_OnIncomingConnectionRequestInfo;
typedef struct _tagEOS_P2P_OnRemoteConnectionClosedInfo EOS_P2P_OnRemoteConnectionClosedInfo;

namespace RN
{
	class EOSP2PClient : public EOSHost
	{
	public:
		EOSAPI EOSP2PClient(bool isHost, String *socketID);
		EOSAPI ~EOSP2PClient() override;

		EOSAPI void Connect(EOS_ProductUserId remoteProductUserID);
		EOSAPI void Disconnect() override;
		EOSAPI void DisconnectClient(EOS_ProductUserId productUserId) override;
		EOSAPI void DisconnectClient(uint8 clientID);
		EOSAPI void DisconnectClientDelayed(uint8 clientID, float delay = 1.0f); //Using this, will not immediately force disconnect the user, leaving some time for previously sent data to arrive (like a reason for getting disconnected)
		EOSAPI void MigrateHost(EOS_ProductUserId hostProductUserId);
		
		EOSAPI bool IsHost() { return _hostClientID != CLIENT_ID_NONE && _hostClientID == _clientID; }

	protected:
		EOSAPI void ReceivedPacketInternal(uint8 *rawData, uint32 bytesWritten, EOS_ProductUserId senderUserID, uint8 channel) final;
		EOSAPI void Update(float delta) override;
		EOSAPI void LogPeers() const;
		EOSAPI virtual void HandleHostMigration(){}
		
		uint8 _hostClientID;

	private:
		static void OnConnectionRequestCallback(const EOS_P2P_OnIncomingConnectionRequestInfo *Data);
		static void OnConnectionClosedCallback(const EOS_P2P_OnRemoteConnectionClosedInfo *Data);

		uint64 _connectionRequestNotificationID;
		uint64 _connectionClosedNotificationID;
		
		uint8 _lastUsedClientID;

		uint8 GetUnusedClientID();
		void AssignClientID(uint8 clientID);
		
		float _connectionTimeout;

		RNDeclareMetaAPI(EOSP2PClient, EOSAPI)
	};
} // namespace RN

#endif /* defined(__RAYNE_EOSP2PCLIENT_H_) */
