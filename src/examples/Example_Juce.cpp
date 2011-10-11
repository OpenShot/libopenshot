//============================================================================
// Name        : LearnJuce.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include "juce.h"

using namespace std;
using namespace juce;


int main() {
	cout << "!!!Hello World!!!" << endl; // prints !!!Hello World!!!

	AudioDeviceManager deviceManager;
	deviceManager.initialise (0, /* number of input channels */
	        2, /* number of output channels */
	        0, /* no XML settings.. */
	        true  /* select default device on failure */);



	///////////// FILE 1 /////////////
	String filepath = "/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/test.wav";
	File audioFile(filepath);

	AudioSourcePlayer audioSourcePlayer;
	deviceManager.addAudioCallback (&audioSourcePlayer);

	// get a format manager and set it up with the basic types (wav and aiff).
	AudioFormatManager formatManager;
	formatManager.registerBasicFormats();

	AudioFormatReader* reader = formatManager.createReaderFor (audioFile);
	ScopedPointer<AudioFormatReaderSource> currentAudioFileSource;
	AudioTransportSource transport1;
	if (reader != 0)
	{
		currentAudioFileSource = new AudioFormatReaderSource (reader, true);

		// ..and plug it into our transport source
		transport1.setSource (currentAudioFileSource,
				32768, // tells it to buffer this many samples ahead
				reader->sampleRate);
		transport1.setPosition (0);
		transport1.setGain(.5);

	}

	///////////// FILE 2 /////////////
	String filepath1 = "/home/jonathan/Aptana Studio Workspace/OpenShotLibrary/src/examples/piano.wav";
	File audioFile1(filepath1);

	AudioSourcePlayer audioSourcePlayer1;
	deviceManager.addAudioCallback (&audioSourcePlayer1);

	// get a format manager and set it up with the basic types (wav and aiff).
	AudioFormatManager formatManager1;
	formatManager1.registerBasicFormats();

	AudioFormatReader* reader1 = formatManager1.createReaderFor (audioFile1);
	ScopedPointer<AudioFormatReaderSource> currentAudioFileSource1;
	AudioTransportSource transport2;
	if (reader1 != 0)
	{
		currentAudioFileSource1 = new AudioFormatReaderSource (reader1, true);

		// ..and plug it into our transport source
		transport2.setSource (currentAudioFileSource1,
			   32768, // tells it to buffer this many samples ahead
			   reader1->sampleRate);
		transport2.setPosition (0);
		transport2.setGain(.5);
	}

	// Create MIXER
	MixerAudioSource mixer;
	mixer.addInputSource(&transport1, true);
	mixer.addInputSource(&transport2, true);
	audioSourcePlayer.setSource (&mixer);

	AudioPluginFormatManager pluginmanager;
	cout << "Number of Plugins: " << pluginmanager.getNumFormats() << endl;


	// Start transports
	transport1.start();
	transport2.start();

    // wait for 10 seconds
    string name = "";
    cout << "Press to Exit" << endl;
    cin >> name;


    transport1.setSource (0);
    audioSourcePlayer.setSource (0);
    deviceManager.removeAudioCallback (&audioSourcePlayer);

	return 0;
}
