#ifndef NETWORK_TIME_SYNC_H
#define NETWORK_TIME_SYNC_H

class CNetSubSystem;

bool ConfigureSystemTimeZone(int offset_minutes);
bool ConfigureDaylightSaving(int offset_minutes, int mode);
void StartNetworkTimeSync(CNetSubSystem *network);

#endif