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

enum ToPrepareMediaType {
	ToPrepareAuto,
	ToPreparePhoto,
	ToPrepareVideo,
	ToPrepareDocument,
};

struct ToPrepareMedia {
	ToPrepareMedia(const QString &file, const PeerId &peer, ToPrepareMediaType t, bool ctrlShiftEnter) : id(MTP::nonce<PhotoId>()), file(file), peer(peer), type(t), ctrlShiftEnter(ctrlShiftEnter) {
	}
	ToPrepareMedia(const QImage &img, const PeerId &peer, ToPrepareMediaType t, bool ctrlShiftEnter) : id(MTP::nonce<PhotoId>()), img(img), peer(peer), type(t), ctrlShiftEnter(ctrlShiftEnter) {
	}
	ToPrepareMedia(const QByteArray &data, const PeerId &peer, ToPrepareMediaType t, bool ctrlShiftEnter) : id(MTP::nonce<PhotoId>()), data(data), peer(peer), type(t), ctrlShiftEnter(ctrlShiftEnter) {
	}
	PhotoId id;
	QString file;
	QImage img;
	QByteArray data;
	PeerId peer;
	ToPrepareMediaType type;
	bool ctrlShiftEnter;
};
typedef QList<ToPrepareMedia> ToPrepareMedias;

typedef QMap<int32, QByteArray> LocalFileParts;
struct ReadyLocalMedia {
	ReadyLocalMedia(ToPrepareMediaType type, const QString &file, const QString &filename, int32 filesize, const QByteArray &data, const uint64 &id, const uint64 &thumbId, const QString &thumbExt, const PeerId &peer, const MTPPhoto &photo, const PreparedPhotoThumbs &photoThumbs, const MTPDocument &document, const QByteArray &jpeg, bool ctrlShiftEnter) :
		type(type), file(file), filename(filename), filesize(filesize), data(data), thumbExt(thumbExt), id(id), thumbId(thumbId), peer(peer), photo(photo), document(document), photoThumbs(photoThumbs), ctrlShiftEnter(ctrlShiftEnter) {
		if (!jpeg.isEmpty()) {
			int32 size = jpeg.size();
			for (int32 i = 0, part = 0; i < size; i += UploadPartSize, ++part) {
				parts.insert(part, jpeg.mid(i, UploadPartSize));
			}
			jpeg_md5.resize(32);
			hashMd5Hex(jpeg.constData(), jpeg.size(), jpeg_md5.data());
		}
	}
	ToPrepareMediaType type;
	QString file, filename;
	int32 filesize;
	QByteArray data;
	QString thumbExt;
	uint64 id, thumbId; // id always file-id of media, thumbId is file-id of thumb ( == id for photos)
	PeerId peer;

	MTPPhoto photo;
	MTPDocument document;
	PreparedPhotoThumbs photoThumbs;
	LocalFileParts parts;
	QByteArray jpeg_md5;

	bool ctrlShiftEnter;
};
typedef QList<ReadyLocalMedia> ReadyLocalMedias;

class LocalImageLoader;
class LocalImageLoaderPrivate : public QObject {
	Q_OBJECT

public:

	LocalImageLoaderPrivate(int32 currentUser, LocalImageLoader *loader, QThread *thread);
	~LocalImageLoaderPrivate();

public slots:

	void prepareImages();

signals:

	void imageReady();
	void imageFailed(quint64 id);

private:

	LocalImageLoader *loader;
	int32 user;

};

class LocalImageLoader : public QObject {
	Q_OBJECT

public:

	LocalImageLoader(QObject *parent);
	void append(const QStringList &files, const PeerId &peer, ToPrepareMediaType t = ToPrepareAuto);
	PhotoId append(const QByteArray &img, const PeerId &peer, ToPrepareMediaType t = ToPrepareAuto);
	PhotoId append(const QImage &img, const PeerId &peer, ToPrepareMediaType t = ToPreparePhoto, bool ctrlShiftEnter = false);
	PhotoId append(const QString &file, const PeerId &peer, ToPrepareMediaType t = ToPrepareAuto);

	QMutex *readyMutex();
	ReadyLocalMedias &readyList();

	QMutex *toPrepareMutex();
	ToPrepareMedias &toPrepareMedias();

	~LocalImageLoader();

public slots:

	void onImageReady();
	void onImageFailed(quint64 id);

signals:

	void imageReady();
	void imageFailed(quint64 id);
	void needToPrepare();

private:

	ReadyLocalMedias ready;
	ToPrepareMedias toPrepare;
	QMutex readyLock, toPrepareLock;
	QThread *thread;
	LocalImageLoaderPrivate *priv;

};
