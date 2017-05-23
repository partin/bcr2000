#include <Windows.h>
#include <iostream>

void dump_devices() {
	std::cout << "midiInGetNumDevs=" << midiInGetNumDevs() << std::endl;
	std::cout << "midiOutGetNumDevs=" << midiOutGetNumDevs() << std::endl;

	const UINT num_in_devs = midiInGetNumDevs();
	for (UINT i = 0; i < num_in_devs; i++) {
		MIDIINCAPS mic;
		if (!midiInGetDevCaps(i, &mic, sizeof(MIDIINCAPS))) {
			std::wcout << "in " << i << ":"
				<< " mid=" << mic.wMid
				<< " pid=" << mic.wPid
				<< " ver=" << mic.vDriverVersion
				<< " " << mic.szPname << std::endl;
		}
	}

	const UINT num_out_devs = midiOutGetNumDevs();
	for (UINT i = 0; i < num_out_devs; i++) {
		MIDIOUTCAPS moc;
		if (!midiOutGetDevCaps(i, &moc, sizeof(MIDIOUTCAPS))) {
			std::wcout << "out " << i << ":"
				<< " mid=" << moc.wMid
				<< " pid=" << moc.wPid
				<< " ver=" << moc.vDriverVersion
				<< " tech=" << moc.wTechnology
				<< " voices=" << moc.wVoices
				<< " notes=" << moc.wNotes
				<< " chanmask=" << moc.wChannelMask
				<< " " << moc.szPname
				<< std::endl;
		}
	}
}
