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
		EOSAPI EOSP2PClient(bool isHost, uint16 maxConnections);
		EOSAPI ~EOSP2PClient();

		EOSAPI void Connect(EOS_ProductUserId remoteProductUserID);
		EOSAPI void Disconnect();
		EOSAPI void DisconnectUser(uint16 userID, uint16 data);
		EOSAPI void DisconnectUserDelayed(uint16 userID, float delay = 1.0f); //Using this, will not immediately force disconnect the user, leaving some time for previously sent data to arrive (like a reason for getting disconnected)
		EOSAPI void DisconnectAll();

	protected:
		EOSAPI virtual void Update(float delta) override;

	private:
		static void OnConnectionRequestCallback(const EOS_P2P_OnIncomingConnectionRequestInfo *Data);
		static void OnConnectionClosedCallback(const EOS_P2P_OnRemoteConnectionClosedInfo *Data);

		void ForceDisconnect(RN::uint16 reason);
		uint16 _maxConnections;
		uint64 _connectionRequestNotificationID;
		uint64 _connectionClosedNotificationID;
		bool _isHost; //This client is hosting the current session

		uint16 GetUnusedUserID() const;

		RNDeclareMetaAPI(EOSP2PClient, EOSAPI)
	};
} // namespace RN

#endif /* defined(__RAYNE_EOSP2PCLIENT_H_) */
