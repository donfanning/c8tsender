#include "playlist.hpp"
#include "chromecast.hpp"
#include "webserver.hpp"
#include "cast_channel.pb.h"
#include <syslog.h>
#include <getopt.h>
#include <fstream>
#include <atomic>

void usage();
extern char* __progname;

const char* ffmpegpath()
{
	if (access("./ffmpeg", F_OK) == 0)
		return "./ffmpeg";
	return "ffmpeg";
}

int main(int argc, char* argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	signal(SIGPIPE, SIG_IGN);
	openlog(NULL, LOG_PID | LOG_PERROR, LOG_DAEMON);

	std::string ip;
	unsigned short port = 8080;
	bool subtitles = false, play = false, exitOnFinish = false;
	std::atomic<bool> done(false);
	Playlist playlist;

	static struct option longopts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "chromecast", required_argument, NULL, 'c' },
		{ "port", required_argument, NULL, 'p' },
		{ "playlist", required_argument, NULL, 'P' },
		{ "play", no_argument, NULL, 'y' },
		{ "subtitles", no_argument, NULL, 'S' },
		{ "shuffle", no_argument, NULL, 's' },
		{ "repeat", no_argument, NULL, 'r' },
		{ "repeat-all", no_argument, NULL, 'R' },
		{ "track", required_argument, NULL, 't' },
		{ "exit-on-finish", no_argument, NULL, 'x' },
		{ NULL, 0, NULL, 0 }
	};

	int ch;
	while ((ch = getopt_long(argc, argv, "hc:p:P:sSrRyt:x", longopts, NULL)) != -1) {
		switch (ch) {
			case 'c':
				ip = optarg;
				break;
			case 'p':
				port = strtoul(optarg, NULL, 10);
				break;
			case 'P':
				{
					std::ifstream input(optarg);
					for (std::string line; getline(input, line);)
						playlist.insert(line);
				}
				break;
			case 's':
				playlist.setShuffle(true);
				break;
			case 'S':
				subtitles = true;
				break;
			case 'r':
				playlist.setRepeat(true);
				break;
			case 'R':
				playlist.setRepeatAll(true);
				break;
			case 'y':
				play = true;
				break;
			case 't':
				playlist.insert(std::string(optarg));
				break;
			case 'x':
				exitOnFinish = true;
				break;
			default:
			case 'h':
				usage();
		}
	}

	if (ip.empty())
		usage();

	ChromeCast chromecast(ip);
	chromecast.init();
	chromecast.setSubtitleSettings(subtitles);
	chromecast.setMediaStatusCallback([&chromecast, &playlist, port, exitOnFinish, &done](const std::string& playerState,
			const std::string& idleReason, const std::string& uuid) -> void {
		syslog(LOG_DEBUG, "mediastatus: %s %s %s", playerState.c_str(), idleReason.c_str(), uuid.c_str());
		if (playerState == "IDLE") {
			if (idleReason == "FINISHED") {
				try {
					std::lock_guard<std::mutex> lock(playlist.getMutex());
					if (exitOnFinish && (playlist.getTracks().rbegin())->getUUID() == uuid) {
						syslog(LOG_DEBUG, "playlist done");
						done = true;
						return;
					}
					PlaylistItem track = playlist.getNextTrack(uuid);
					std::thread foo([&chromecast, port, track]() {
						chromecast.load("http://" + chromecast.getSocketName() + ":" + std::to_string(port) + "/stream/" + track.getUUID(),
							track.getName(), track.getUUID());
					});
					foo.detach();
				} catch (...) {
					// ...
				}
			}
		}
	});
	Webserver http(port, chromecast, playlist);
	if (play) {
		try {
			const PlaylistItem& track = playlist.getNextTrack();
			chromecast.load(
				"http://" + chromecast.getSocketName() + ":" + std::to_string(port) + "/stream/" + track.getUUID(),
				track.getName(), track.getUUID());
		} catch (const std::runtime_error& e) {
			syslog(LOG_DEBUG, "--play failed: %s", e.what());
		}
	}
	while (!done) sleep(1);
	return 0;
}

void usage()
{
	printf("%s --chromecast <ip> [ --port <number> ] [ --playlist <path> ]\n"
			"\t[ --shuffle ] [ --repeat ] [ --repeat-all ]\n"
			"\t[ --subtitles ] [ --play ] [ --track <file> ]\n", __progname);
	exit(1);
}
