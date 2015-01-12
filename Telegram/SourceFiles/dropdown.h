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

#include "gui/twidget.h"
#include "gui/boxshadow.h"

class Dropdown : public TWidget, public Animated {
	Q_OBJECT

public:

	Dropdown(QWidget *parent);

	IconedButton *addButton(IconedButton *button);
	void resetButtons();

	void resizeEvent(QResizeEvent *e);
	void paintEvent(QPaintEvent *e);

	void enterEvent(QEvent *e);
	void leaveEvent(QEvent *e);
	void otherEnter();
	void otherLeave();

	void fastHide();

	bool animStep(float64 ms);

	bool eventFilter(QObject *obj, QEvent *e);

public slots:

	void hideStart();
	void hideFinish();

	void showStart();

private:

	void adjustButtons();

	typedef QVector<IconedButton*> Buttons;
	Buttons _buttons;

	int32 _width, _height;
	bool _hiding;

	anim::fvalue a_opacity;

	QTimer _hideTimer;

	BoxShadow _shadow;

};

class DragArea : public TWidget, public Animated {
	Q_OBJECT

public:

	DragArea(QWidget *parent);

	void paintEvent(QPaintEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void dragEnterEvent(QDragEnterEvent *e);
	void dragLeaveEvent(QDragLeaveEvent *e);
	void dropEvent(QDropEvent *e);
	void dragMoveEvent(QDragMoveEvent *e);

	void setText(const QString &text, const QString &subtext);

	void otherEnter();
	void otherLeave();

	void fastHide();

	bool animStep(float64 ms);

signals:

	void dropped(QDropEvent *e);

public slots:

	void hideStart();
	void hideFinish();

	void showStart();

private:

	bool _hiding, _in;

	anim::fvalue a_opacity;
	anim::cvalue a_color;

	BoxShadow _shadow;

	QString _text, _subtext;

};

class EmojiPanInner : public QWidget, public Animated {
	Q_OBJECT

public:

	EmojiPanInner(QWidget *parent = 0);

	void paintEvent(QPaintEvent *e);

	void mousePressEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void leaveEvent(QEvent *e);

	bool animStep(float64 ms);

	void showEmojiPack(DBIEmojiTab packIndex);

	void clearSelection(bool fast = false);
	
public slots:

	void updateSelected();
	void onSaveConfig();

signals:

	void emojiSelected(EmojiPtr emoji);
	void stickerSelected(DocumentData *sticker);

private:

	typedef QMap<int32, uint64> EmojiAnimations; // index - showing, -index - hiding
	EmojiAnimations _emojiAnimations;

	StickerPack _stickers;
	QVector<bool> _isUserGen;
	QVector<EmojiPtr> _emojis;
	QVector<float64> _hovers;

	DBIEmojiTab _tab;
	int32 _selected, _xSelected, _pressedSel, _xPressedSel;
	QPoint _lastMousePos;

	QTimer _saveConfigTimer;

};

class EmojiPan : public TWidget, public Animated {
	Q_OBJECT

public:

	EmojiPan(QWidget *parent);

	void paintEvent(QPaintEvent *e);

	void enterEvent(QEvent *e);
	void leaveEvent(QEvent *e);
	void otherEnter();
	void otherLeave();

	void fastHide();
	bool hiding() const {
		return _hiding || _hideTimer.isActive();
	}

	bool animStep(float64 ms);

	bool eventFilter(QObject *obj, QEvent *e);

public slots:

	void hideStart();
	void hideFinish();

	void showStart();

	void onTabChange();

signals:

	void emojiSelected(EmojiPtr emoji);
	void stickerSelected(DocumentData *sticker);
	void updateStickers();

private:

	void showAll();
	void hideAll();

	int32 _width, _height;
	bool _hiding;
	QPixmap _cache;

	anim::fvalue a_opacity;

	QTimer _hideTimer;

	BoxShadow _shadow;

	FlatRadiobutton _recent, _people, _nature, _objects, _places, _symbols, _stickers;

	int32 _emojiPack;
	ScrollArea _scroll;
	EmojiPanInner _inner;

};

//class StickerPanInner : public QWidget, public Animated {
//	Q_OBJECT
//
//public:
//
//	StickerPanInner(QWidget *parent = 0);
//
//	void paintEvent(QPaintEvent *e);
//
//	void mousePressEvent(QMouseEvent *e);
//	void mouseReleaseEvent(QMouseEvent *e);
//	void mouseMoveEvent(QMouseEvent *e);
//	void leaveEvent(QEvent *e);
//
//	bool animStep(float64 ms);
//
//	void showStickerPack(EmojiPtr emoji);
//	bool hasContent() const;
//
//public slots:
//
//	void updateSelected();
//
//signals:
//
//	void stickerSelected(DocumentData *sticker);
//
//private:
//
//	typedef QMap<int32, uint64> StickerAnimations; // index - showing, -index - hiding
//	StickerAnimations _stickerAnimations;
//
//	StickerPack _stickers;
//	QVector<float64> _hovers;
//
//	EmojiPtr _emoji;
//	int32 _selected, _pressedSel;
//	QPoint _lastMousePos;
//
//};
//
//class StickerPan : public TWidget, public Animated {
//	Q_OBJECT
//
//public:
//
//	StickerPan(QWidget *parent);
//
//	void setStickerPack(EmojiPtr emoji, bool show);
//	void paintEvent(QPaintEvent *e);
//
//	void enterEvent(QEvent *e);
//	void leaveEvent(QEvent *e);
//	void otherEnter();
//	void otherLeave();
//
//	void fastHide();
//
//	bool animStep(float64 ms);
//
//	bool eventFilter(QObject *obj, QEvent *e);
//
//public slots:
//
//	void hideStart();
//	void hideFinish();
//
//	void showStart();
//
//signals:
//
//	void stickerSelected(DocumentData *sticker);
//
//private:
//
//	void showAll();
//	void hideAll();
//
//	int32 _width, _height;
//	bool _hiding;
//	QPixmap _cache;
//
//	anim::fvalue a_opacity;
//
//	QTimer _hideTimer;
//
//	BoxShadow _shadow;
//
//	EmojiPtr _emoji;
//	ScrollArea _scroll;
//	StickerPanInner _inner;
//
//};
