#pragma once

#include <iostream>
#include <vector>
#include <functional>
#include <memory>

#include <windows.h>
#include <tchar.h>

class BCR2000;

class midi_out {
	friend class BCR2000;

private:
	bool is_open = false;
	HMIDIOUT midi_out_handle;

	static void CALLBACK MidiOutProc(HMIDIOUT hMidiOut, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2) {}

public:
	midi_out(BCR2000 & bcr) : m_bcr(bcr) {}

	BCR2000 & m_bcr;

	int find_device(const TCHAR * name) {
		const UINT num_out_devs = midiOutGetNumDevs();
		for (UINT i = 0; i < num_out_devs; i++) {
			MIDIOUTCAPS moc;
			if (!midiOutGetDevCaps(i, &moc, sizeof(MIDIOUTCAPS))) {
				if (std::basic_string<TCHAR>(moc.szPname) == name) {
					return i;
				}
			}
		}
		return -1;
	}

	bool open(const TCHAR * name) {
		const int device_id = find_device(name);
		MMRESULT res = midiOutOpen(&midi_out_handle, device_id, (DWORD_PTR)(void*)MidiOutProc, (DWORD_PTR)(&m_bcr), CALLBACK_FUNCTION);
		if (res != 0) {
			return false;
		}
		is_open = true;
		return true;
	}
	void close() {
		midiOutClose(midi_out_handle);
		is_open = false;
	}
	bool control_change(char channel, char controller, char value) {
		DWORD msg = 0xb0 | channel | (controller << 8) | (value << 16);
		MMRESULT res = midiOutShortMsg(midi_out_handle, msg);
		if (res != MMSYSERR_NOERROR) {
			return false;
		}
		return true;
	}
	bool sysex(int size, char * data) {
		MIDIHDR midiHeader;
		midiHeader.dwFlags = 0;
		midiHeader.lpData = data;
		midiHeader.dwBufferLength = size;

		MMRESULT res = midiOutPrepareHeader(midi_out_handle, &midiHeader, sizeof(midiHeader));
		if (res != MMSYSERR_NOERROR) {
			return false;
		}
		res = midiOutLongMsg(midi_out_handle, &midiHeader, sizeof(midiHeader));
		if (res != MMSYSERR_NOERROR) {
			return false;
		}
		res = midiOutUnprepareHeader(midi_out_handle, &midiHeader, sizeof(midiHeader));
		if (res != MMSYSERR_NOERROR) {
			return false;
		}
		return true;
	}
	~midi_out() {
		if (is_open)
			close();
	}
};

class midi_in {
	friend class BCR2000;

private:
	midi_in(BCR2000 & bcr) : m_bcr(bcr) {}

	BCR2000 & m_bcr;
	bool is_open = false;
	HMIDIIN midi_in_handle;

	static void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);

public:
	int find_device(const TCHAR * name) {
		const UINT num_in_devs = midiInGetNumDevs();
		for (UINT i = 0; i < num_in_devs; i++) {
			MIDIINCAPS mic;
			if (!midiInGetDevCaps(i, &mic, sizeof(MIDIINCAPS))) {
				if (std::basic_string<TCHAR>(mic.szPname) == name) {
					return i;
				}
			}
		}
		return -1;
	}

	bool open(const TCHAR * name) {
		const int device_id = find_device(name);
		if (device_id == -1) {
			return false;
		}

		MMRESULT res = midiInOpen(&midi_in_handle, device_id, reinterpret_cast<DWORD_PTR>(MidiInProc), reinterpret_cast<DWORD_PTR>(&m_bcr), CALLBACK_FUNCTION);
		if (res != 0)
			return false;
		is_open = true;
		midiInStart(midi_in_handle);
		return true;
	}
	void close() {
		midiInStop(midi_in_handle);
		midiInClose(midi_in_handle);
		is_open = false;
	}
	~midi_in() {
		if (is_open)
			close();
	}
};


class BCR2000 {
private:
	struct Listener {
		Listener(std::function<void(int)> trigger_, bool hires = false) : m_trigger(trigger_), m_hires(hires) {}
		bool m_hires = false;
		void trigger(int data) { m_trigger(data); }
		std::function<void(int)> m_trigger;
	};

	struct bcl_msg_t {
		int idx;
		std::vector<char> data;

		bcl_msg_t() : idx(0) {}

		void operator()(const char *format, ...) {
			data.push_back('\xf0'); // SysEx
			data.push_back('\x00'); data.push_back(0x20); data.push_back(0x32); // Manufacturer
			data.push_back('\x7f'); // DeviceID: any
			data.push_back('\x15'); // Model: BCR2000
			data.push_back('\x20'); // Command: BCL message
			data.push_back(static_cast<char>((idx & 0xff00) >> 8));
			data.push_back(static_cast<char>(idx & 0xff));

			char buffer[80];
			va_list argptr;
			va_start(argptr, format);
			vsnprintf(buffer, sizeof(buffer), format, argptr);
			va_end(argptr);

			for (char *ptr = buffer; *ptr != '\0'; ++ptr) {
				data.push_back(*ptr);
			}

			data.push_back('\xf7'); // End
			++idx;
		}

		int size() {
			return static_cast<int>(data.size());
		}

		void show() {
			for (size_t i = 0; i < data.size(); ++i) {
				std::cout << std::hex << (int)data[i] << " ";
			}
			std::cout << std::endl;
		}
	};

public:
	BCR2000(int channel) : m_midiIn(*this), m_midiOut(*this), m_channel(channel) {
		if (!m_midiIn.open(_T("BCR2000"))) {
			m_isConnected = false;
			return;
		}
		if (!m_midiOut.open(_T("BCR2000"))) {
			m_isConnected = false;
			return;
		}
		m_isConnected = true;
	}

	void AddButton(int button, std::function<void(void)> callback) {
		bcl_msg_t m;
		m("$rev R1");
		m("$button %d", button);
		m("  .showvalue on");
		m("  .easypar CC %d %d 0 1 toggleon", m_channel + 1, m_controller);
		m("  .mode down");
		m("$end");
		m_midiOut.sysex(m.size(), &m.data[0]);
		m_listener[m_controller] = std::unique_ptr<Listener>(new Listener([callback](int data) { callback(); }));
		++m_controller;
	}

	void AddToggleButton(int button, std::function<void(bool)> callback) {
		bcl_msg_t m;
		m("$rev R1");
		m("$button %d", button);
		m("  .showvalue on");
		m("  .easypar CC %d %d 0 1 toggleoff", m_channel + 1, m_controller);
		m("  .mode toggle");
		m("$end");
		m_midiOut.sysex(m.size(), &m.data[0]);
		m_listener[m_controller] = std::unique_ptr<Listener>(new Listener([callback](int data) { callback(data == 0); }));
		++m_controller;
	}

	void AddOnOffButton(int button, std::function<void(bool)> callback) {
		bcl_msg_t m;
		m("$rev R1");
		m("$button %d", button);
		m("  .showvalue on");
		m("  .easypar CC %d %d 0 1 toggleoff", m_channel + 1, m_controller);
		m("  .mode updown");
		m("$end");
		m_midiOut.sysex(m.size(), &m.data[0]);
		m_listener[m_controller] = std::unique_ptr<Listener>(new Listener([callback](int data) { callback(data == 0); }));
		++m_controller;
	}

	void AddRelativeEncoder(int encoder, std::function<void(int)> callback) {
		bcl_msg_t m;
		m("$rev R1");
		m("$encoder %d", encoder);
		m("  .showvalue off");
		m("  .easypar CC %d %d 0 127 relative-2", m_channel + 1, m_controller);
		m("  .resolution 96 192 768 2304");
		m("  .mode 1dot/off");
		m("$end");
		m_midiOut.sysex(m.size(), &m.data[0]);
		m_listener[m_controller] = std::unique_ptr<Listener>(new Listener([callback](int data) { callback(data - 64); }));
		++m_controller;
	}

	void AddAbsoluteEncoder(int encoder, int minVal, int maxVal, bool hires, std::function<void(int)> callback) {
		bcl_msg_t m;
		m("$rev R1");
		m("$encoder %d", encoder);
		m("  .showvalue on");
		m("  .easypar CC %d %d %d %d absolute%s", m_channel + 1, m_controller, minVal, maxVal, hires ? "/14" : "");
		m("  .mode bar");
		m("$end");
		m_midiOut.sysex(m.size(), &m.data[0]);
		m_listener[m_controller] = std::unique_ptr<Listener>(new Listener([callback](int data) { callback(data); }, hires));
		++m_controller;
	}

	void data(int channel, int controller, int data) {
		if (channel != m_channel)
			return;

		int realCtrl = controller;
		if (controller >= 32 && controller <= 63 && m_listener[controller - 32] && m_listener[controller - 32]->m_hires) {
			realCtrl -= 32;
		}

		if (!m_listener[realCtrl]) {
			return;
		}

		if (!m_listener[realCtrl]->m_hires) {
			m_listener[realCtrl]->trigger(data);
			return;
		}

		if (controller == realCtrl) {
			data <<= 7;
		}

		if (m_waiting[realCtrl]) {
			m_waiting[realCtrl] = false;
			m_listener[realCtrl]->trigger(m_value[realCtrl] + data);
			return;
		}
		else {
			m_waiting[realCtrl] = true;
			m_value[realCtrl] = data;
			return;
		}

	}

	bool IsConnected() { return m_isConnected; }

private:
	int m_channel = 1;
	int m_controller = 0;
	std::unique_ptr<Listener> m_listener[127];
	int m_value[127] = { 0 };
	bool m_waiting[127] = { false };
	midi_in m_midiIn;
	midi_out m_midiOut;
	bool m_isConnected;
};

void CALLBACK midi_in::MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
	if (wMsg == MIM_DATA) {
		BCR2000 * bcr = (BCR2000 *) dwInstance;
		enum { CHANNEL_MASK = 0x0f };
		enum { TYPE_MASK = 0xf0 };
		enum { TYPE_CONTROL_CHANGE = 0xb0 };
		if ((dwParam1 & TYPE_MASK) == TYPE_CONTROL_CHANGE) {
			bcr->data(dwParam1 & CHANNEL_MASK, (dwParam1 & 0xff00) >> 8, (dwParam1 & 0xff0000) >> 16);
		}
	}
	return;
}