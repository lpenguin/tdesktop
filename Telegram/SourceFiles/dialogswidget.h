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
#pragma once

class MainWidget;

class DialogsListWidget : public QWidget {
	Q_OBJECT

public:

	DialogsListWidget(QWidget *parent, MainWidget *main);

	void dialogsReceived(const QVector<MTPDialog> &dialogs);
	void searchReceived(const QVector<MTPMessage> &messages, bool fromStart, int32 fullCount);
	void peopleReceived(const QString &query, const QVector<MTPContactFound> &people);
	void showMore(int32 pixels);

	void activate();

	void contactsReceived(const QVector<MTPContact> &contacts);
	int32 addNewContact(int32 uid, bool sel = false); // return y of row or -1 if failed

	void paintEvent(QPaintEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void mousePressEvent(QMouseEvent *e);
	void resizeEvent(QResizeEvent *e);
	void enterEvent(QEvent *e);
	void leaveEvent(QEvent *e);

	void peopleResultPaint(UserData *user, QPainter &p, int32 w, bool act, bool sel) const;

	void selectSkip(int32 direction);
	void selectSkipPage(int32 pixels, int32 direction);

	void createDialogAtTop(History *history, int32 unreadCount);
	void dlgUpdated(DialogRow *row);
	void dlgUpdated(History *row);
	void removePeer(PeerData *peer);
	void removeContact(UserData *user);

	void loadPeerPhotos(int32 yFrom);
	void clearFilter();
	void refresh(bool toTop = false);

	bool choosePeer();

	void destroyData();

	void peerBefore(const PeerData *inPeer, MsgId inMsg, PeerData *&outPeer, MsgId &outMsg) const;
	void peerAfter(const PeerData *inPeer, MsgId inMsg, PeerData *&outPeer, MsgId &outMsg) const;
	void scrollToPeer(const PeerId &peer, MsgId msgId);

	typedef QVector<DialogRow*> FilteredDialogs;
	typedef QVector<UserData*> PeopleResults;
	typedef QVector<FakeDialogRow*> SearchResults;

	DialogsIndexed &contactsList();
	DialogsIndexed &dialogsList();
	FilteredDialogs &filteredList();
	PeopleResults &peopleList();
	SearchResults &searchList();
	MsgId lastSearchId() const;

	void setMouseSel(bool msel, bool toTop = false);

	enum State {
		DefaultState = 0,
		FilteredState = 1,
		SearchedState = 2,
	};
	void setState(State newState);
	State state() const;

	void onFilterUpdate(QString newFilter, bool force = false);
	void itemRemoved(HistoryItem *item);
	void itemReplaced(HistoryItem *oldItem, HistoryItem *newItem);

	~DialogsListWidget();

public slots:

	void onUpdateSelected(bool force = false);
	void onParentGeometryChanged();
	void onDialogToTop(const History::DialogLinks &links);
	void onPeerNameChanged(PeerData *peer, const PeerData::Names &oldNames, const PeerData::NameFirstChars &oldChars);
	void onPeerPhotoChanged(PeerData *peer);
	void onDialogRowReplaced(DialogRow *oldRow, DialogRow *newRow);

signals:

	void mustScrollTo(int scrollToTop, int scrollToBottom);
	void dialogToTopFrom(int movedFrom);
	void searchMessages();
	void searchResultChosen();

private:

	void addDialog(const MTPDdialog &dialog);
	void clearSearchResults(bool clearPeople = true);

	DialogsIndexed dialogs;
	DialogsIndexed contactsNoDialogs;
	DialogsIndexed contacts;
	DialogRow *sel;
	bool contactSel;
	bool selByMouse;

	QString filter;
	FilteredDialogs filterResults;
	int32 filteredSel;

	SearchResults searchResults;
	int32 searchedCount, searchedSel;

	QString peopleQuery;
	PeopleResults peopleResults;
	int32 peopleSel;

	MsgId _lastSearchId;

	State _state;

	QPoint lastMousePos;

	void paintDialog(QPainter &p, DialogRow *dialog);

	LinkButton _addContactLnk;

};

class DialogsWidget : public QWidget, public Animated, public RPCSender {
	Q_OBJECT

public:
	DialogsWidget(MainWidget *parent);

	void dialogsReceived(const MTPmessages_Dialogs &dialogs);
	void contactsReceived(const MTPcontacts_Contacts &contacts);
	void searchReceived(bool fromStart, const MTPmessages_Messages &result, mtpRequestId req);
	void peopleReceived(const MTPcontacts_Found &result, mtpRequestId req);
	bool addNewContact(int32 uid, bool show = true);
	
	void resizeEvent(QResizeEvent *e);
	void keyPressEvent(QKeyEvent *e);
	void paintEvent(QPaintEvent *e);

	void loadDialogs();
	void createDialogAtTop(History *history, int32 unreadCount);
	void dlgUpdated(DialogRow *row);
	void dlgUpdated(History *row);

	void dialogsToUp();

	void regTyping(History *history, UserData *user);

	bool animStep(float64 ms);

	void setInnerFocus();

	void destroyData();

	void peerBefore(const PeerData *inPeer, MsgId inMsg, PeerData *&outPeer, MsgId &outMsg) const;
	void peerAfter(const PeerData *inPeer, MsgId inMsg, PeerData *&outPeer, MsgId &outMsg) const;
	void scrollToPeer(const PeerId &peer, MsgId msgId);

	void removePeer(PeerData *peer);
	void removeContact(UserData *user);

	DialogsIndexed &contactsList();

	void enableShadow(bool enable = true);
	
	void searchMessages(const QString &query);
	void onSearchMore(MsgId minMsgId);

	void itemRemoved(HistoryItem *item);
	void itemReplaced(HistoryItem *oldItem, HistoryItem *newItem);

signals:

	void cancelled();

public slots:

	void onCancel();
	void onListScroll();
	void activate();
	void onFilterUpdate();
	void onAddContact();
	void onNewGroup();
	void onCancelSearch();

	void onDialogToTopFrom(int movedFrom);
	bool onSearchMessages(bool searchCache = false);
	void onNeedSearchMessages();

private:

	bool _drawShadow;

	void unreadCountsReceived(const QVector<MTPDialog> &dialogs);
	bool dialogsFailed(const RPCError &e);
	bool contactsFailed();
	bool searchFailed(const RPCError &error, mtpRequestId req);
	bool peopleFailed(const RPCError &error, mtpRequestId req);

	int32 dlgOffset, dlgCount;
	mtpRequestId dlgPreloading;
	mtpRequestId contactsRequest;

	FlatInput _filter;
	IconedButton _newGroup, _addContact, _cancelSearch;
	ScrollArea scroll;
	DialogsListWidget list;

	QTimer _searchTimer;
	QString _searchQuery, _peopleQuery;
	bool _searchFull, _peopleFull;
	mtpRequestId _searchRequest, _peopleRequest;

	typedef QMap<QString, MTPmessages_Messages> SearchCache;
	SearchCache _searchCache;

	typedef QMap<mtpRequestId, QString> SearchQueries;
	SearchQueries _searchQueries;

	typedef QMap<QString, MTPcontacts_Found> PeopleCache;
	PeopleCache _peopleCache;

	typedef QMap<mtpRequestId, QString> PeopleQueries;
	PeopleQueries _peopleQueries;

};
