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

void audioInit();
bool audioWorks();
void audioPlayNotify();
void audioFinish();

enum VoiceMessageState {
	VoiceMessageStopped,
	VoiceMessageStarting,
	VoiceMessagePlaying,
	VoiceMessageFinishing,
	VoiceMessagePausing,
	VoiceMessagePaused,
	VoiceMessageResuming,
};

class VoiceMessagesFader;
class VoiceMessagesLoader;

class VoiceMessages : public QObject {
	Q_OBJECT

public:

	VoiceMessages();

	void play(AudioData *audio);
	void pauseresume();

	void currentState(AudioData **audio, VoiceMessageState *state = 0, int64 *position = 0, int64 *duration = 0);

	~VoiceMessages();

public slots:

	void onError(AudioData *audio);

signals:

	void updated(AudioData *audio);
	void stopped(AudioData *audio);

	void faderOnTimer();
	void loaderOnStart(AudioData *audio);
	void loaderOnCancel(AudioData *audio);

private:

	bool updateCurrentStarted(int32 pos = -1);

	struct Msg {
		Msg() : audio(0), position(0), duration(0), skipStart(0), skipEnd(0), loading(0), started(0),
		state(VoiceMessageStopped), source(0), nextBuffer(0) {
			memset(buffers, 0, sizeof(buffers));
			memset(samplesCount, 0, sizeof(samplesCount));
		}
		AudioData *audio;
		QString fname;
		QByteArray data;
		int64 position, duration;
		int64 skipStart, skipEnd;
		bool loading;
		int64 started;
		VoiceMessageState state;

		uint32 source;
		int32 nextBuffer;
		uint32 buffers[3];
		int64 samplesCount[3];
	};

	int32 _current;
	Msg _data[AudioVoiceMsgSimultaneously];

	QMutex _mutex;

	friend class VoiceMessagesFader;
	friend class VoiceMessagesLoader;

	QThread _faderThread;
	QThread _loaderThread;
	VoiceMessagesFader *_fader;
	VoiceMessagesLoader *_loader;

};

VoiceMessages *audioVoice();

class VoiceMessagesFader : public QObject {
	Q_OBJECT

public:

	VoiceMessagesFader(QThread *thread);

signals:

	void error(AudioData *audio);
	void playPositionUpdated(AudioData *audio);
	void audioStopped(AudioData *audio);
	void needToPreload(AudioData *audio);

public slots:

	void onInit();
	void onTimer();

private:

	QTimer _timer;

};

class VoiceMessagesLoader : public QObject {
	Q_OBJECT

public:

	VoiceMessagesLoader(QThread *thread);
	~VoiceMessagesLoader();

signals:

	void error(AudioData *audio);
	void needToCheck();

public slots:

	void onInit();
	void onStart(AudioData *audio);
	void onLoad(AudioData *audio);
	void onCancel(AudioData *audio);
	
private:

	struct Loader;
	typedef QMap<AudioData*, Loader*> Loaders;
	Loaders _loaders;

	void loadError(Loaders::iterator i);

};
