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

#include "layerwidget.h"
#include "localimageloader.h"

class PhotoSendBox : public LayeredWidget {
	Q_OBJECT

public:

	PhotoSendBox(const ReadyLocalMedia &img);
	PhotoSendBox(const QString &phone, const QString &fname, const QString &lname);
	void parentResized();
	void animStep(float64 ms);
	void keyPressEvent(QKeyEvent *e);
	void paintEvent(QPaintEvent *e);
	void startHide();
	~PhotoSendBox();

public slots:

	void onSend(bool ctrlShiftEnter = false);
	void onCancel();

private:

	ReadyLocalMedia *_img;
	int32 _width, _height, _thumbx, _thumby, _thumbw, _thumbh;
	QString _name, _size;
	int32 _namew, _textw;
	FlatCheckbox _compressed;
	FlatButton _sendButton, _cancelButton;
	QPixmap _thumb;

	QString _phone, _fname, _lname;

	anim::fvalue a_opacity;

	bool _hiding;

};
