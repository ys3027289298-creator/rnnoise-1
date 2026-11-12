#include <stdio.h>
#include "rnnoise.h"
int main(int c,char**v){FILE*a=fopen(v[1],"rb"),*b=fopen(v[2],"wb");
DenoiseState*s=rnnoise_sr_create(0,48000);float x[480];short t[480];int i;
while(fread(t,2,480,a)==480){for(i=0;i<480;i++)x[i]=t[i];
rnnoise_sr_process_frame(s,x,x);for(i=0;i<480;i++)t[i]=x[i];fwrite(t,2,480,b);}
rnnoise_sr_destroy(s);return 0;}
