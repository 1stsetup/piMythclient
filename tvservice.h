
int tvserviceInit();
void tvserviceDestroy();
int tvserviceHDMIPowerOn(int use3D);
int tvserviceHDMIPowerOnBest(int use3D, uint32_t width, uint32_t height, uint32_t frame_rate, int interlaced);
int tvservicePowerOff();
int tvserviceAVLatency();
