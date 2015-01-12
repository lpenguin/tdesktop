/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014 John Preston, https://desktop.telegram.org
*/
#include "stdafx.h"
#include "mtp.h"

#include "localstorage.h"

namespace {
	typedef QMap<int32, MTProtoSessionPtr> Sessions;
	Sessions sessions;
	MTProtoSessionPtr mainSession;

	typedef QMap<mtpRequestId, int32> RequestsByDC; // holds dc for request to this dc or -dc for request to main dc
	RequestsByDC requestsByDC;
	QMutex requestByDCLock;

	typedef QMap<mtpRequestId, int32> AuthExportRequests; // holds target dc for auth export request
	AuthExportRequests authExportRequests;

	bool started = false;

	uint32 layer;
	
	typedef QMap<mtpRequestId, RPCResponseHandler> ParserMap;
	ParserMap parserMap;
	QMutex parserMapLock;

	typedef QMap<mtpRequestId, mtpRequest> RequestMap;
	RequestMap requestMap;
	QReadWriteLock requestMapLock;

	typedef QPair<mtpRequestId, uint64> DelayedRequest;
	typedef QList<DelayedRequest> DelayedRequestsList;
	DelayedRequestsList delayedRequests;

	typedef QMap<mtpRequestId, int32> RequestsDelays;
	RequestsDelays requestsDelays;

	typedef QSet<mtpRequestId> BadGuestDCRequests;
	BadGuestDCRequests badGuestDCRequests;

	typedef QVector<mtpRequestId> DCAuthWaiters;
	typedef QMap<int32, DCAuthWaiters> AuthWaiters;
	AuthWaiters authWaiters;

	QMutex toClearLock;
	RPCCallbackClears toClear;

	RPCResponseHandler globalHandler;
	MTPStateChangedHandler stateChangedHandler = 0;
	MTPSessionResetHandler sessionResetHandler = 0;
	_mtp_internal::RequestResender *resender = 0;

	void importDone(const MTPauth_Authorization &result, mtpRequestId req) {
		QMutexLocker locker1(&requestByDCLock);

		RequestsByDC::iterator i = requestsByDC.find(req);
		if (i == requestsByDC.end()) {
			LOG(("MTP Error: auth import request not found in requestsByDC, requestId: %1").arg(req));
			RPCError error(rpcClientError("AUTH_IMPORT_FAIL", QString("did not find import request in requestsByDC, request %1").arg(req)));
			if (globalHandler.onFail && MTP::authedId()) (*globalHandler.onFail)(req, error); // auth failed in main dc
			return;
		}
		int32 newdc = i.value();

		DEBUG_LOG(("MTP Info: auth import to dc %1 succeeded").arg(newdc));

		DCAuthWaiters &waiters(authWaiters[newdc]);
		MTProtoSessionPtr session(_mtp_internal::getSession(newdc));
		if (waiters.size()) {
			QReadLocker locker(&requestMapLock);
			for (DCAuthWaiters::iterator i = waiters.begin(), e = waiters.end(); i != e; ++i) {
				mtpRequestId requestId = *i;
				RequestMap::const_iterator j = requestMap.constFind(requestId);
				if (j == requestMap.cend()) {
					LOG(("MTP Error: could not find request %1 for resending").arg(requestId));
					continue;
				}
				{
					RequestsByDC::iterator k = requestsByDC.find(requestId);
					if (k == requestsByDC.cend()) {
						LOG(("MTP Error: could not find request %1 by dc for resending").arg(requestId));
						continue;
					}
					if (k.value() < 0) {
						MTP::setdc(newdc);
						k.value() = -newdc;
					} else {
						k.value() = k.value() - (k.value() % _mtp_internal::dcShift) + newdc;
					}
					DEBUG_LOG(("MTP Info: resending request %1 to dc %2 after import auth").arg(requestId).arg(k.value()));
				}
				session->sendPrepared(j.value());
			}
			waiters.clear();
		}
	}

	bool importFail(const RPCError &error, mtpRequestId req) {
		if (globalHandler.onFail && MTP::authedId()) (*globalHandler.onFail)(req, error); // auth import failed
		return true;
	}

	void exportDone(const MTPauth_ExportedAuthorization &result, mtpRequestId req) {
		AuthExportRequests::const_iterator i = authExportRequests.constFind(req);
		if (i == authExportRequests.cend()) {
			LOG(("MTP Error: auth export request target dc not found, requestId: %1").arg(req));
			RPCError error(rpcClientError("AUTH_IMPORT_FAIL", QString("did not find target dc, request %1").arg(req)));
			if (globalHandler.onFail && MTP::authedId()) (*globalHandler.onFail)(req, error); // auth failed in main dc
			return;
		}

		const MTPDauth_exportedAuthorization &data(result.c_auth_exportedAuthorization());
		MTP::send(MTPauth_ImportAuthorization(data.vid, data.vbytes), rpcDone(importDone), rpcFail(importFail), i.value());
		authExportRequests.remove(req);
	}

	bool exportFail(const RPCError &error, mtpRequestId req) {
		AuthExportRequests::const_iterator i = authExportRequests.constFind(req);
		if (i != authExportRequests.cend()) {
			authWaiters[i.value()].clear();
		}
		if (globalHandler.onFail && MTP::authedId()) (*globalHandler.onFail)(req, error); // auth failed in main dc
		return true;
	}

	bool onErrorDefault(mtpRequestId requestId, const RPCError &error) {
		const QString &err(error.type());
		int32 code = error.code();
		bool badGuestDC = (code == 400) && (err == qsl("FILE_ID_INVALID"));
        QRegularExpressionMatch m;
		if ((m = QRegularExpression("^(FILE|PHONE|NETWORK|USER)_MIGRATE_(\\d+)$").match(err)).hasMatch()) {
			if (!requestId) return false;

			int32 dc = 0, newdc = m.captured(2).toInt();
			{
				QMutexLocker locker(&requestByDCLock);
				RequestsByDC::iterator i = requestsByDC.find(requestId);
				if (i == requestsByDC.end()) {
					LOG(("MTP Error: could not find request %1 for migrating to %2").arg(requestId).arg(newdc));
				} else {
					dc = i.value();
				}
			}
			if (!dc || !newdc) return false;

			DEBUG_LOG(("MTP Info: changing request %1 dc%2 to %3").arg(requestId).arg((dc > 0) ? "" : " and main dc").arg(newdc));
			if (dc < 0) {
				if (MTP::authedId()) { // import auth, set dc and resend
					DEBUG_LOG(("MTP Info: importing auth to dc %1").arg(newdc));
					DCAuthWaiters &waiters(authWaiters[newdc]);
					if (!waiters.size()) {
						authExportRequests.insert(MTP::send(MTPauth_ExportAuthorization(MTP_int(newdc)), rpcDone(exportDone), rpcFail(exportFail)), newdc);
					}
					waiters.push_back(requestId);
					return true;
				} else {
					MTP::setdc(newdc);
				}
			}

			mtpRequest req;
			{
				QReadLocker locker(&requestMapLock);
				RequestMap::const_iterator i = requestMap.constFind(requestId);
				if (i == requestMap.cend()) {
					LOG(("MTP Error: could not find request %1").arg(requestId));
					return false;
				}
				req = i.value();
			}
			_mtp_internal::registerRequest(requestId, (dc < 0) ? -newdc : newdc);
			_mtp_internal::getSession(newdc)->sendPrepared(req);
			return true;
		} else if (code < 0 || code >= 500 || (m = QRegularExpression("^FLOOD_WAIT_(\\d+)$").match(err)).hasMatch()) {
			if (!requestId) return false;
			
			int32 secs = 1;
			if (code < 0 || code >= 500) {
				RequestsDelays::iterator i = requestsDelays.find(requestId);
				if (i != requestsDelays.cend()) {
					secs = (i.value() > 60) ? i.value() : (i.value() *= 2);
				} else {
					requestsDelays.insert(requestId, secs);
				}
			} else {
				secs = m.captured(1).toInt();
			}
			uint64 sendAt = getms(true) + secs * 1000 + 10;
			DelayedRequestsList::iterator i = delayedRequests.begin(), e = delayedRequests.end();
			for (; i != e; ++i) {
				if (i->first == requestId) return true;
				if (i->second > sendAt) break;
			}
			delayedRequests.insert(i, DelayedRequest(requestId, sendAt));

			if (resender) resender->checkDelayed();

			return true;
		} else if (code == 401 || (badGuestDC && badGuestDCRequests.constFind(requestId) == badGuestDCRequests.cend())) {
			int32 dc = 0;
			{
				QMutexLocker locker(&requestByDCLock);
				RequestsByDC::iterator i = requestsByDC.find(requestId);
				if (i != requestsByDC.end()) {
					dc = i.value();
				} else {
					LOG(("MTP Error: unauthorized request without dc info, requestId %1").arg(requestId));
				}
			}
			int32 newdc = abs(dc) % _mtp_internal::dcShift;
			if (!newdc || newdc == mtpMainDC() || !MTP::authedId()) {
				if (!badGuestDC && globalHandler.onFail) (*globalHandler.onFail)(requestId, error); // auth failed in main dc
				return false;
			}

			DEBUG_LOG(("MTP Info: importing auth to dc %1").arg(dc));
			DCAuthWaiters &waiters(authWaiters[newdc]);
			if (!waiters.size()) {
				authExportRequests.insert(MTP::send(MTPauth_ExportAuthorization(MTP_int(newdc)), rpcDone(exportDone), rpcFail(exportFail)), newdc);
			}
			waiters.push_back(requestId);
			if (badGuestDC) badGuestDCRequests.insert(requestId);
			return true;
		} else if (err == qsl("CONNECTION_NOT_INITED") || err == qsl("CONNECTION_LAYER_INVALID")) {
			mtpRequest req;
			{
				QReadLocker locker(&requestMapLock);
				RequestMap::const_iterator i = requestMap.constFind(requestId);
				if (i == requestMap.cend()) {
					LOG(("MTP Error: could not find request %1").arg(requestId));
					return false;
				}
				req = i.value();
			}
			int32 dc = 0;
			{
				QMutexLocker locker(&requestByDCLock);
				RequestsByDC::iterator i = requestsByDC.find(requestId);
				if (i == requestsByDC.end()) {
					LOG(("MTP Error: could not find request %1 for resending with init connection").arg(requestId));
				} else {
					dc = i.value();
				}
			}
			if (!dc) return false;

			req->needsLayer = true;
			_mtp_internal::getSession(dc < 0 ? (-dc) : dc)->sendPrepared(req);
			return true;
		} else if (err == qsl("MSG_WAIT_FAILED")) {
			mtpRequest req;
			{
				QReadLocker locker(&requestMapLock);
				RequestMap::const_iterator i = requestMap.constFind(requestId);
				if (i == requestMap.cend()) {
					LOG(("MTP Error: could not find request %1").arg(requestId));
					return false;
				}
				req = i.value();
			}
			if (!req->after) {
				LOG(("MTP Error: wait failed for not dependent request %1").arg(requestId));
				return false;
			}
			int32 dc = 0;
			{
				QMutexLocker locker(&requestByDCLock);
				RequestsByDC::iterator i = requestsByDC.find(requestId), j = requestsByDC.find(req->after->requestId);
				if (i == requestsByDC.end()) {
					LOG(("MTP Error: could not find request %1 by dc").arg(requestId));
				} else if (j == requestsByDC.end()) {
					LOG(("MTP Error: could not find dependent request %1 by dc").arg(req->after->requestId));
				} else {
					dc = i.value();
					if (i.value() != j.value()) {
						req->after = mtpRequest();
					}
				}
			}
			if (!dc) return false;

			if (!req->after) {
				req->needsLayer = true;
				_mtp_internal::getSession(dc < 0 ? (-dc) : dc)->sendPrepared(req);
			} else {
				int32 newdc = abs(dc) % _mtp_internal::dcShift;
				DCAuthWaiters &waiters(authWaiters[newdc]);
				if (waiters.indexOf(req->after->requestId) >= 0) {
					if (waiters.indexOf(requestId) < 0) {
						waiters.push_back(requestId);
					}
					if (badGuestDCRequests.constFind(req->after->requestId) != badGuestDCRequests.cend()) {
						if (badGuestDCRequests.constFind(requestId) == badGuestDCRequests.cend()) {
							badGuestDCRequests.insert(requestId);
						}
					}
				} else {
					uint64 at = 0;
					DelayedRequestsList::iterator i = delayedRequests.begin(), e = delayedRequests.end();
					for (; i != e; ++i) {
						if (i->first == requestId) return true;
						if (i->first == req->after->requestId) break;
					}
					if (i != e) {
						delayedRequests.insert(i, DelayedRequest(requestId, i->second));
					}

					if (resender) resender->checkDelayed();
				}
			}
			return true;
		}
		if (badGuestDC) badGuestDCRequests.remove(requestId);
		return false;
	}

}

namespace _mtp_internal {
	MTProtoSessionPtr getSession(int32 dc) {
		if (!started) return MTProtoSessionPtr();
		if (!dc) return mainSession;
		if (!(dc % _mtp_internal::dcShift)) {
			dc += mainSession->getDC();
		}
		
		Sessions::const_iterator i = sessions.constFind(dc);
		if (i != sessions.cend()) return *i;

		MTProtoSessionPtr result(new MTProtoSession());
		result->start(dc);
		
		sessions.insert(dc, result);
		return result;
	}
	
	void registerRequest(mtpRequestId requestId, int32 dc) {
		{
			QMutexLocker locker(&requestByDCLock);
			requestsByDC.insert(requestId, dc);
		}
		_mtp_internal::performDelayedClear(); // need to do it somewhere..
	}

	void unregisterRequest(mtpRequestId requestId) {
		requestsDelays.remove(requestId);

		{
			QWriteLocker locker(&requestMapLock);
			requestMap.remove(requestId);
		}

		QMutexLocker locker(&requestByDCLock);
		requestsByDC.remove(requestId);
	}

	mtpRequestId storeRequest(mtpRequest &request, const RPCResponseHandler &parser) {
		mtpRequestId res = reqid();
		request->requestId = res;
		if (parser.onDone || parser.onFail) {
			QMutexLocker locker(&parserMapLock);
			parserMap.insert(res, parser);
		}
		{
			QWriteLocker locker(&requestMapLock);
			requestMap.insert(res, request);
		}
		return res;
	}

	mtpRequest getRequest(mtpRequestId reqId) {
		static mtpRequest zero;
		mtpRequest req;
		{
			QReadLocker locker(&requestMapLock);
			RequestMap::const_iterator i = requestMap.constFind(reqId);
			req = (i == requestMap.cend()) ? zero : i.value();
		}
		return req;
	}

	void wrapInvokeAfter(mtpRequest &to, const mtpRequest &from, const mtpRequestMap &haveSent, int32 skipBeforeRequest) {
		mtpMsgId afterId(*(mtpMsgId*)(from->after->data() + 4));
		mtpRequestMap::const_iterator i = afterId ? haveSent.constFind(afterId) : haveSent.cend();
		int32 size = to->size(), lenInInts = (from.innerLength() >> 2), headlen = 4, fulllen = headlen + lenInInts;
		if (i == haveSent.constEnd()) { // no invoke after or such msg was not sent or was completed recently
			to->resize(size + fulllen + skipBeforeRequest);
			if (skipBeforeRequest) {
				memcpy(to->data() + size, from->constData() + 4, headlen * sizeof(mtpPrime));
				memcpy(to->data() + size + headlen + skipBeforeRequest, from->constData() + 4 + headlen, lenInInts * sizeof(mtpPrime));
			} else {
				memcpy(to->data() + size, from->constData() + 4, fulllen * sizeof(mtpPrime));
			}
		} else {
			to->resize(size + fulllen + skipBeforeRequest + 3);
			memcpy(to->data() + size, from->constData() + 4, headlen * sizeof(mtpPrime));
			(*to)[size + 3] += 3 * sizeof(mtpPrime);
			*((mtpTypeId*)&((*to)[size + headlen + skipBeforeRequest])) = mtpc_invokeAfterMsg;
			memcpy(to->data() + size + headlen + skipBeforeRequest + 1, &afterId, 2 * sizeof(mtpPrime));
			memcpy(to->data() + size + headlen + skipBeforeRequest + 3, from->constData() + 4 + headlen, lenInInts * sizeof(mtpPrime));
			if (size + 3 != 7) (*to)[7] += 3 * sizeof(mtpPrime);
		}
	}

	void clearCallbacks(mtpRequestId requestId, int32 errorCode) {
		RPCResponseHandler h;
		bool found = false;
		{
			QMutexLocker locker(&parserMapLock);
			ParserMap::iterator i = parserMap.find(requestId);
			if (i != parserMap.end()) {
				h = i.value();
				found = true;

				parserMap.erase(i);
			}
		}
		if (errorCode && found) {
			rpcErrorOccured(requestId, h, rpcClientError("CLEAR_CALLBACK", QString("did not handle request %1, error code %2").arg(requestId).arg(errorCode)));
		}
	}

	void clearCallbacksDelayed(const RPCCallbackClears &requestIds) {
		uint32 idsCount = requestIds.size();
		if (!idsCount) return;

		if (cDebug()) {
			QString idsStr = QString("%1").arg(requestIds[0].requestId);
			for (uint32 i = 1; i < idsCount; ++i) {
				idsStr += QString(", %1").arg(requestIds[i].requestId);
			}
			DEBUG_LOG(("RPC Info: clear callbacks delayed, msgIds: %1").arg(idsStr));
		}

		QMutexLocker lock(&toClearLock);
		uint32 toClearNow = toClear.size();
		if (toClearNow) {
			toClear.resize(toClearNow + idsCount);
			memcpy(toClear.data() + toClearNow, requestIds.constData(), idsCount * sizeof(RPCCallbackClear));
		} else {
			toClear = requestIds;
		}
	}

	void performDelayedClear() {
		QMutexLocker lock(&toClearLock);
		if (!toClear.isEmpty()) {
			for (RPCCallbackClears::iterator i = toClear.begin(), e = toClear.end(); i != e; ++i) {
				if (cDebug()) {
					QMutexLocker locker(&parserMapLock);
					if (parserMap.find(i->requestId) != parserMap.end()) {
						DEBUG_LOG(("RPC Info: clearing delayed callback %1, error code %2").arg(i->requestId).arg(i->errorCode));
					}
				}
				clearCallbacks(i->requestId, i->errorCode);
				_mtp_internal::unregisterRequest(i->requestId);
			}
			toClear.clear();
		}
	}

	void execCallback(mtpRequestId requestId, const mtpPrime *from, const mtpPrime *end) {
		RPCResponseHandler h;
		{
			QMutexLocker locker(&parserMapLock);
			ParserMap::iterator i = parserMap.find(requestId);
			if (i != parserMap.cend()) {
				h = i.value();
				parserMap.erase(i);

				DEBUG_LOG(("RPC Info: found parser for request %1, trying to parse response..").arg(requestId));
			}
		}
		if (h.onDone || h.onFail) {
			try {
				if (from >= end) throw mtpErrorInsufficient();

				if (*from == mtpc_rpc_error) {
					RPCError err(MTPRpcError(from, end));
					DEBUG_LOG(("RPC Info: error received, code %1, type %2, description: %3").arg(err.code()).arg(err.type()).arg(err.description()));
					if (!rpcErrorOccured(requestId, h, err)) {
						QMutexLocker locker(&parserMapLock);
						parserMap.insert(requestId, h);
						return;
					}
				} else {
					if (h.onDone) (*h.onDone)(requestId, from, end);
				}
			} catch (Exception &e) {
				if (!rpcErrorOccured(requestId, h, rpcClientError("RESPONSE_PARSE_FAILED", QString("exception text: ") + e.what()))) {
					QMutexLocker locker(&parserMapLock);
					parserMap.insert(requestId, h);
					return;
				}
			}
		} else {
			DEBUG_LOG(("RPC Info: parser not found for %1").arg(requestId));
		}
		unregisterRequest(requestId);
	}

	bool hasCallbacks(mtpRequestId requestId) {
		QMutexLocker locker(&parserMapLock);
		ParserMap::iterator i = parserMap.find(requestId);
		return (i != parserMap.cend());
	}

	void globalCallback(const mtpPrime *from, const mtpPrime *end) {
		if (globalHandler.onDone) (*globalHandler.onDone)(0, from, end); // some updates were received
	}

	void onStateChange(int32 dc, int32 state) {
		if (stateChangedHandler) stateChangedHandler(dc, state);
	}

	void onSessionReset(int32 dc) {
		if (sessionResetHandler) sessionResetHandler(dc);
	}

	bool rpcErrorOccured(mtpRequestId requestId, const RPCFailHandlerPtr &onFail, const RPCError &err) { // return true if need to clean request data
		if (onErrorDefault(requestId, err)) {
			return false;
		}
		LOG(("RPC Error: request %1 got fail with code %2, error %3%4").arg(requestId).arg(err.code()).arg(err.type()).arg(err.description().isEmpty() ? QString() : QString(": %1").arg(err.description())));
		onFail && (*onFail)(requestId, err);
		return true;
	}

	RequestResender::RequestResender() {
		connect(&_timer, SIGNAL(timeout()), this, SLOT(checkDelayed()));
	}

	void RequestResender::checkDelayed() {
		uint64 now = getms(true);
		while (!delayedRequests.isEmpty() && now >= delayedRequests.front().second) {
			mtpRequestId requestId = delayedRequests.front().first;
			delayedRequests.pop_front();

			int32 dc = 0;
			{
				QMutexLocker locker(&requestByDCLock);
				RequestsByDC::const_iterator i = requestsByDC.constFind(requestId);
				if (i != requestsByDC.cend()) {
					dc = i.value();
				} else {
					LOG(("MTP Error: could not find request dc for delayed resend, requestId %1").arg(requestId));
					continue;
				}
			}

			mtpRequest req;
			{
				QReadLocker locker(&requestMapLock);
				RequestMap::const_iterator j = requestMap.constFind(requestId);
				if (j == requestMap.cend()) {
					DEBUG_LOG(("MTP Error: could not find request %1").arg(requestId));
					continue;
				}
				req = j.value();
			}
			_mtp_internal::getSession(dc < 0 ? (-dc) : dc)->sendPrepared(req);
		}

		if (!delayedRequests.isEmpty()) {
			_timer.start(delayedRequests.front().second - now);
		}
	}
};

namespace MTP {

    void start() {
        unixtimeInit();

		if (!Local::oldKey().created()) {
			LOG(("App Error: trying to start MTP without local key!"));
			return;
		}

		mtpLoadData();
		MTProtoDCMap &dcs(mtpDCMap());

		mainSession = MTProtoSessionPtr(new MTProtoSession());
		mainSession->start(mtpMainDC());
		sessions[mainSession->getDC()] = mainSession;

		started = true;
		resender = new _mtp_internal::RequestResender();

		if (mtpNeedConfig()) {
			mtpConfigLoader()->load();
		}
	}

	void restart() {
		if (!started) return;

		for (Sessions::const_iterator i = sessions.cbegin(), e = sessions.cend(); i != e; ++i) {
			(*i)->restart();
		}
	}
	void restart(int32 dcMask) {
		if (!started) return;

		for (Sessions::const_iterator i = sessions.cbegin(), e = sessions.cend(); i != e; ++i) {
			if ((*i)->getDC() % _mtp_internal::dcShift == dcMask % _mtp_internal::dcShift) {
				(*i)->restart();
			}
		}
	}

	void setdc(int32 dc, bool fromZeroOnly) {
		if (!started) return;

		int32 m = mainSession->getDC();
		if (!dc || m == dc || (m && fromZeroOnly)) return;
		mtpSetDC(dc);
		mainSession = _mtp_internal::getSession(dc);
	}

	int32 maindc() {
		if (!started) return 0;

		return mainSession->getDC();
	}

	int32 dcstate(int32 dc) {
		if (!started) return 0;

		if (!dc) return mainSession->getState();
		if (!(dc % _mtp_internal::dcShift)) {
			dc += mainSession->getDC();
		}
		
		Sessions::const_iterator i = sessions.constFind(dc);
		if (i != sessions.cend()) return (*i)->getState();

		return MTProtoConnection::Disconnected;
	}

	QString dctransport(int32 dc) {
		if (!started) return QString();

		if (!dc) return mainSession->transport();
		if (!(dc % _mtp_internal::dcShift)) {
			dc += mainSession->getDC();
		}

		Sessions::const_iterator i = sessions.constFind(dc);
		if (i != sessions.cend()) return (*i)->transport();

		return QString();
	}

	void initdc(int32 dc) {
		if (!started) return;
		_mtp_internal::getSession(dc);
	}

	void cancel(mtpRequestId requestId) {
		mtpMsgId msgId = 0;
		requestsDelays.remove(requestId);
		{
			QWriteLocker locker(&requestMapLock);
			RequestMap::iterator i = requestMap.find(requestId);
			if (i != requestMap.end()) {
				msgId = *(mtpMsgId*)(i.value()->constData() + 4);
				requestMap.erase(i);
			}
		}
		{
			QMutexLocker locker(&requestByDCLock);
			RequestsByDC::iterator i = requestsByDC.find(requestId);
			if (i != requestsByDC.end()) {
				_mtp_internal::getSession(abs(i.value()))->cancel(requestId, msgId);
				requestsByDC.erase(i);
			}
		}
		_mtp_internal::clearCallbacks(requestId);
	}

	void killSession(int32 dc) {
		Sessions::iterator i = sessions.find(dc);
		if (i != sessions.end()) {
			bool wasMain = (i.value() == mainSession);

			i.value()->stop();
			sessions.erase(i);

			if (wasMain) {
				mainSession = MTProtoSessionPtr(new MTProtoSession());
				mainSession->start(mtpMainDC());
				sessions[mainSession->getDC()] = mainSession;
			}
		}
	}

	void stopSession(int32 dc) {
		Sessions::iterator i = sessions.find(dc);
		if (i != sessions.end()) {
			if (i.value() != mainSession) { // don't stop main session
				i.value()->stop();
			}
		}
	}

	int32 state(mtpRequestId requestId) {
		if (requestId > 0) {
			QMutexLocker locker(&requestByDCLock);
			RequestsByDC::iterator i = requestsByDC.find(requestId);
			if (i != requestsByDC.end()) {
				return _mtp_internal::getSession(abs(i.value()))->requestState(requestId);
			}
			return MTP::RequestSent;
		}
		return _mtp_internal::getSession(-requestId)->requestState(0);
	}

	void stop() {
		for (Sessions::iterator i = sessions.begin(), e = sessions.end(); i != e; ++i) {
			i.value()->stop();
		}
		sessions.clear();
		mainSession = MTProtoSessionPtr();
		delete resender;
		resender = 0;
		mtpDestroyConfigLoader();
	}

	void authed(int32 uid) {
		mtpAuthed(uid);
	}

	int32 authedId() {
		return mtpAuthed();
	}

	void logoutKeys(RPCDoneHandlerPtr onDone, RPCFailHandlerPtr onFail) {
		mtpRequestId req = MTP::send(MTPauth_LogOut(), onDone, onFail);
		mtpLogoutOtherDCs();
	}

	void setGlobalDoneHandler(RPCDoneHandlerPtr handler) {
		globalHandler.onDone = handler;
	}

	void setGlobalFailHandler(RPCFailHandlerPtr handler) {
		globalHandler.onFail = handler;
	}

	void setStateChangedHandler(MTPStateChangedHandler handler) {
		stateChangedHandler = handler;
	}

	void setSessionResetHandler(MTPSessionResetHandler handler) {
		sessionResetHandler = handler;
	}

	void clearGlobalHandlers() {
		setGlobalDoneHandler(RPCDoneHandlerPtr());
		setGlobalFailHandler(RPCFailHandlerPtr());
		setStateChangedHandler(0);
		setSessionResetHandler(0);
	}

	void updateDcOptions(const QVector<MTPDcOption> &options) {
		mtpUpdateDcOptions(options);
		App::writeUserConfig();
	}

	void writeConfig(QDataStream &stream) {
		return mtpWriteConfig(stream);
	}

	bool readConfigElem(int32 blockId, QDataStream &stream) {
		return mtpReadConfigElem(blockId, stream);
	}

};
