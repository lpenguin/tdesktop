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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "title.h"
#include "pspecific.h"
#include "gui/boxshadow.h"

class MediaView;
class TitleWidget;
class IntroWidget;
class MainWidget;
class SettingsWidget;
class BackgroundWidget;
class LayeredWidget;
namespace Local {
	class ClearManager;
}

class ConnectingWidget : public QWidget {
	Q_OBJECT

public:

	ConnectingWidget(QWidget *parent, const QString &text, const QString &reconnect);
	void set(const QString &text, const QString &reconnect);
	void paintEvent(QPaintEvent *e);

public slots:

	void onReconnect();

private:

	BoxShadow _shadow;
	QString _text;
	int32 _textWidth;
	LinkButton _reconnect;

};

class NotifyWindow : public QWidget, public Animated {
	Q_OBJECT

public:

	NotifyWindow(HistoryItem *item, int32 x, int32 y);

	void enterEvent(QEvent *e);
	void leaveEvent(QEvent *e);
	void mousePressEvent(QMouseEvent *e);
	void paintEvent(QPaintEvent *e);

	bool animStep(float64 ms);
	void animHide(float64 duration, anim::transition func);
	void startHiding();
	void stopHiding();
	void moveTo(int32 x, int32 y, int32 index = -1);

	void updateNotifyDisplay();
	void updatePeerPhoto();

	void itemRemoved(HistoryItem *del);

	int32 index() const {
		return history ? _index : -1;
	}

	void unlinkHistory(History *hist = 0);

	~NotifyWindow();

public slots:

	void hideByTimer();
	void checkLastInput();

	void unlinkHistoryAndNotify();

private:

#ifdef Q_OS_WIN
	DWORD started;
#endif
	History *history;
	HistoryItem *item;
	IconedButton close;
	QPixmap pm;
	float64 alphaDuration, posDuration;
	QTimer hideTimer, inputTimer;
	bool hiding;
	int32 _index;
	anim::fvalue aOpacity;
	anim::transition aOpacityFunc;
	anim::ivalue aY;
	ImagePtr peerPhoto;

};

typedef QList<NotifyWindow*> NotifyWindows;

class Window : public PsMainWindow {
	Q_OBJECT

public:
	Window(QWidget *parent = 0);
	~Window();

	void init();
	void firstShow();

	QWidget *filedialogParent();

	bool eventFilter(QObject *obj, QEvent *evt);

	void inactivePress(bool inactive);
	bool inactivePress() const;

	void wStartDrag(QMouseEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *e);
	void closeEvent(QCloseEvent *e);

	void paintEvent(QPaintEvent *e);

	void resizeEvent(QResizeEvent *e);
	void updateWideMode();
	bool needBackButton();

	void setupIntro(bool anim);
	void setupMain(bool anim);
	void startMain(const MTPUser &user);
	void getNotifySetting(const MTPInputNotifyPeer &peer, uint32 msWait = 0);
	void serviceNotification(const QString &msg, bool unread = true, const MTPMessageMedia &media = MTP_messageMediaEmpty(), bool force = false);
	void sendServiceHistoryRequest();
	void showDelayedServiceMsgs();

	void mtpStateChanged(int32 dc, int32 state);

	TitleWidget *getTitle();

	HitTestType hitTest(const QPoint &p) const;
	QRect iconRect() const;

	QRect clientRect() const;
	QRect photoRect() const;
	
	IntroWidget *introWidget();
	MainWidget *mainWidget();
	SettingsWidget *settingsWidget();

	void showConnecting(const QString &text, const QString &reconnect = QString());
	void hideConnecting();
	bool connectingVisible() const;

	void showPhoto(const PhotoLink *lnk, HistoryItem *item = 0);
	void showPhoto(PhotoData *photo, HistoryItem *item);
	void showPhoto(PhotoData *photo, PeerData *item);
	void showDocument(DocumentData *doc, QPixmap pix, HistoryItem *item);
	void showLayer(LayeredWidget *w, bool fast = false);
	void replaceLayer(LayeredWidget *w);
	void hideLayer(bool fast = false);
	bool hideInnerLayer();

	bool layerShown();

	bool historyIsActive(int state = -1) const;

	void activate();

	void noIntro(IntroWidget *was);
	void noSettings(SettingsWidget *was);
	void noMain(MainWidget *was);
	void noBox(BackgroundWidget *was);
	void layerFinishedHide(BackgroundWidget *was);

	void topWidget(QWidget *w);
	void noTopWidget(QWidget *w);

	void fixOrder();

	enum TempDirState {
		TempDirRemoving,
		TempDirExists,
		TempDirEmpty,
	};
	TempDirState tempDirState();
	TempDirState localStorageState();
	void tempDirDelete(int task);

	void quit();

    void notifySettingGot();
	void notifySchedule(History *history, MsgId msgId);
	void notifyClear(History *history = 0);
	void notifyClearFast();
	void notifyShowNext(NotifyWindow *remove = 0);
	void notifyItemRemoved(HistoryItem *item);
	void notifyStopHiding();
	void notifyStartHiding();
	void notifyUpdateAllPhotos();
	void notifyUpdateAll();
	void notifyActivateAll();

	QImage iconLarge() const;

	void sendPaths();

	void mediaOverviewUpdated(PeerData *peer);
	void changingMsgId(HistoryItem *row, MsgId newId);

public slots:
	
	void checkHistoryActivation(int state = -1);
	void updateCounter();
    
	void showSettings();
	void hideSettings(bool fast = false);
	void layerHidden();
	void updateTitleStatus();

	void quitFromTray();
	void showFromTray(QSystemTrayIcon::ActivationReason reason = QSystemTrayIcon::Unknown);
	bool minimizeToTray();
	void toggleTray(QSystemTrayIcon::ActivationReason reason = QSystemTrayIcon::Unknown);

	void onInactiveTimer();

	void onClearFinished(int task, void *manager);
	void onClearFailed(int task, void *manager);

	void notifyFire();
	void updateTrayMenu(bool force = false);

	void onShowAddContact();
	void onShowNewGroup();
	void onLogout();
	void onLogoutSure();
	void updateGlobalMenu(); // for OS X top menu

	QImage iconWithCounter(int size, int count, style::color bg, bool smallIcon);

signals:

	void resized(const QSize &size);
	void tempDirCleared(int task);
	void tempDirClearFailed(int task);

protected:

	void setupTrayIcon();

private:

	void placeSmallCounter(QImage &img, int size, int count, style::color bg, const QPoint &shift, style::color color);
	QImage icon16, icon32, icon64, iconbig16, iconbig32, iconbig64;

	QWidget *centralwidget;

	typedef QPair<QPair<QString, MTPMessageMedia>, bool> DelayedServiceMsg;
	QVector<DelayedServiceMsg> _delayedServiceMsgs;
	mtpRequestId _serviceHistoryRequest;

	TitleWidget *title;
	IntroWidget *intro;
	MainWidget *main;
	SettingsWidget *settings;
	BackgroundWidget *layerBG;

	QWidget *_topWidget; // temp hack for CountrySelect
	ConnectingWidget *_connecting;

	Local::ClearManager *_clearManager;

	void clearWidgets();

	bool dragging;
	QPoint dragStart;

	bool _inactivePress;
	QTimer _inactiveTimer;

	typedef QMap<MsgId, uint64> NotifyWhenMap;
	typedef QMap<History*, NotifyWhenMap> NotifyWhenMaps;
	NotifyWhenMaps notifyWhenMaps;
	struct NotifyWaiter {
		NotifyWaiter(MsgId msg, uint64 when) : msg(msg), when(when) {
		}
		MsgId msg;
		uint64 when;
	};
	typedef QMap<History*, NotifyWaiter> NotifyWaiters;
	NotifyWaiters notifyWaiters;
	NotifyWaiters notifySettingWaiters;
	SingleTimer notifyWaitTimer;

	typedef QMap<uint64, NullType> NotifyWhenAlert;
	typedef QMap<History*, NotifyWhenAlert> NotifyWhenAlerts;
	NotifyWhenAlerts notifyWhenAlerts;

	NotifyWindows notifyWindows;

	MediaView *_mediaView;
};

#endif // MAINWINDOW_H
