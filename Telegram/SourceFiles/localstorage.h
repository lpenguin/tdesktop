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

#include "types.h"

namespace _local_inner {

	class Manager : public QObject {
	Q_OBJECT

	public:

		Manager();

		void writeMap(bool fast);
		void writingMap();
		void writeLocations(bool fast);
		void writingLocations();
		void finish();

	public slots:

		void mapWriteTimeout();
		void locationsWriteTimeout();

	private:

		QTimer _mapWriteTimer;
		QTimer _locationsWriteTimer;

	};

}

namespace Local {

	mtpAuthKey &oldKey();
	void createOldKey(QByteArray *salt = 0);

	void start();
	void stop();
	
	enum ClearManagerTask {
		ClearManagerAll = 0xFFFF,
		ClearManagerDownloads = 0x01,
		ClearManagerStorage = 0x02,
	};

	struct ClearManagerData;
	class ClearManager : public QObject {
		Q_OBJECT

	public:
		ClearManager();
		bool addTask(int task);
		bool hasTask(ClearManagerTask task);
		void start();
		~ClearManager();

	public slots:
		void onStart();

	signals:
		void succeed(int task, void *manager);
		void failed(int task, void *manager);

	private:
		ClearManagerData *data;

	};

	enum ReadMapState {
		ReadMapFailed = 0,
		ReadMapDone = 1,
		ReadMapPassNeeded = 2,
	};
	ReadMapState readMap(const QByteArray &pass);
	int32 oldMapVersion();

	void writeDraft(const PeerId &peer, const QString &text);
	QString readDraft(const PeerId &peer);
	void writeDraftPositions(const PeerId &peer, const MessageCursor &cur);
	MessageCursor readDraftPositions(const PeerId &peer);
	bool hasDraftPositions(const PeerId &peer);

	void writeFileLocation(const StorageKey &location, const FileLocation &local);
	FileLocation readFileLocation(const StorageKey &location, bool check = true);

	void writeImage(const StorageKey &location, const ImagePtr &img);
	void writeImage(const StorageKey &location, const StorageImageSaved &jpeg, bool overwrite = true);
	StorageImageSaved readImage(const StorageKey &location);
	int32 hasImages();
	qint64 storageImagesSize();

	void writeSticker(const StorageKey &location, const QByteArray &data, bool overwrite = true);
	QByteArray readSticker(const StorageKey &location);
	int32 hasStickers();
	qint64 storageStickersSize();

	void writeAudio(const StorageKey &location, const QByteArray &data, bool overwrite = true);
	QByteArray readAudio(const StorageKey &location);
	int32 hasAudios();
	qint64 storageAudiosSize();

	void writeRecentStickers();
	void readRecentStickers();

};
