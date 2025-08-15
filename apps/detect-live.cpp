#include <helper.h>
#include <iostream>
#include <algorithm>
#include <snowboy-detect.h>
#include "pulseaudio.hh"

namespace pa = pulseaudio::pa;

int main(int argc, const char** argv) try
{
	const auto root = detect_project_root();
	std::string model = root + "resources/models/snowboy.umdl";
	if(argc > 1) model = argv[1];
	pa::simple_record_stream audio_in{"Microphone input"};
	pa::simple_playback_stream audio_out{"Ding"};

	// Determine hotword name from model file
	std::string hotword = "snowboy";  // default
	if (model.find("computer.umdl") != std::string::npos) hotword = "computer";
	else if (model.find("jarvis.umdl") != std::string::npos) hotword = "jarvis";
	else if (model.find("hey_extreme.umdl") != std::string::npos) hotword = "hey extreme";
	else if (model.find("alexa.umdl") != std::string::npos) hotword = "alexa";
	else if (model.find("hey_casper.pmdl") != std::string::npos) hotword = "hey casper";
	else if (model.find(".pmdl") != std::string::npos) {
		// Extract filename from path for .pmdl files
		size_t lastSlash = model.find_last_of("/\\");
		size_t lastDot = model.find_last_of(".");
		if (lastSlash != std::string::npos && lastDot != std::string::npos) {
			hotword = model.substr(lastSlash + 1, lastDot - lastSlash - 1);
			// Replace underscores with spaces
			std::replace(hotword.begin(), hotword.end(), '_', ' ');
		}
	}

	snowboy::SnowboyDetect detector(root + "resources/common.res", model);
	detector.SetSensitivity("0.5");  // More reasonable sensitivity
	detector.SetAudioGain(1.5);      // Moderate gain
	detector.ApplyFrontend(true);

	auto ding = read_sample_file(root + "resources/dong.wav");
	std::vector<short> samples;
	
	std::cout << "Listening for hotword '" << hotword << "'... (Press Ctrl+C to exit)" << std::endl;
	std::cout << "Sensitivity: 0.5, Audio Gain: 1.5, Using model: " << model << std::endl;
	
	int loop_count = 0;
	while (true) {
		audio_in.read(samples);
		auto s = detector.RunDetection(samples.data(), samples.size(), false);
		
		// Calculate audio level for debugging
		int max_sample = 0;
		for (auto sample : samples) {
			max_sample = std::max(max_sample, abs(sample));
		}
		
		// Show activity indicator and audio level every 50 loops (~3 seconds)
		if (++loop_count % 50 == 0) {
			std::cout << "." << "[" << max_sample << "]" << std::flush;
		}
		
		// Show detection result
		std::cout << "\r   \r" << s << std::flush;
		
		if (s > 0) {
			std::cout << "\n*** HOTWORD DETECTED! *** (confidence: " << s << ")" << std::endl;
			audio_out.write(ding);
			std::cout << "Listening again..." << std::endl;
		}
	}
	return 0;
} catch (const std::exception& e) {
	std::cerr << "Error: " << e.what() << std::endl;
	return -1;
}
