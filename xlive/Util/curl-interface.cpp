#include "stdafx.h"
#include "curl-interface.h"


CURL* curl_interface_init()
{
	// LAN edition: all internet access (accounts, master server/server list, updater,
	// h2maps.net map downloads, achievements, stats, MOTD) is disabled here.
	// Every caller handles a null handle as "network unavailable".
	return nullptr;
}

CURL* curl_interface_init_no_verify()
{
	CURL* curl = curl_interface_init();
	if (curl != nullptr)
	{

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Cartographer " DLL_VERSION_STR);

		return curl;
	}

	return nullptr;
}