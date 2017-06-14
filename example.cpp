#include "src/bcr2000.h"

#include <iostream>
#include <conio.h>

#pragma comment(lib, "winmm.lib")

using namespace std;

int main() {
	BCR2000 bcr(1);

	if (!bcr.IsConnected()) {
		return 0;
	}

	// BCR 2000 layout:

	// encoder group 1: 1-8    buttons: 1-8
	// encoder group 2: 9-16   buttons: 9-16
	// encoder group 3: 17-24  buttons: 17-24
	// encoder group 4: 25-32  buttons: 25-32

	// buttons: 33-40
	// buttons: 41-48

	// encoders: 33-40
	// encoders: 41-48
	// encoders: 49-56


	// button, fires at push down
	bcr.AddButton(33, []() { cout << "button 33" << endl; });

	// button, toggles state at each click
	bcr.AddToggleButton(34, [](bool state) { cout << "toggle 34: " << state << endl; });

	// button, sends true at push down, false when released
	bcr.AddOnOffButton(35, [](bool state) { cout << "onoff 35: " << state << endl; });

	// 7-bit absolute encoder, range 0-127
	bcr.AddAbsoluteEncoder(1, 0, 127, false, [](int val) { cout << "abs 0-127: " << val << endl; });

	// relative encoder
	bcr.AddRelativeEncoder(2, [](int val) { cout << "rel: " << val << endl; });

	// 14-bit absolute encoder, range 500-700
	bcr.AddAbsoluteEncoder(33, 500, 700, true, [](int val) { cout << "abs 500-700: " << val << endl; });

	getch();
	return 0;
}
