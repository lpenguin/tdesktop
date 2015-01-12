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

class TWidget : public QWidget {
	Q_OBJECT

public:

	TWidget(QWidget *parent = 0) : QWidget(parent) {
	}
	TWidget *tparent() {
		return dynamic_cast<TWidget*>(parentWidget());
	}
	const TWidget *tparent() const {
		return dynamic_cast<const TWidget*>(parentWidget());
	}

	virtual void leaveToChildEvent(QEvent *e) { // e -- from enterEvent() of child TWidget
	}
	virtual void enterFromChildEvent(QEvent *e) { // e -- from leaveEvent() of child TWidget
	}

	bool event(QEvent *e) {
		return QWidget::event(e);
	}

protected:

	void enterEvent(QEvent *e) {
		TWidget *p(tparent());
		if (p) p->leaveToChildEvent(e);
	}
	void leaveEvent(QEvent *e) {
		TWidget *p(tparent());
		if (p) p->enterFromChildEvent(e);
	}

private:

};

QPixmap myGrab(QWidget *target, const QRect &rect);
