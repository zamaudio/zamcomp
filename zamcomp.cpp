#include <lv2plugin.hpp>
#include <stdint.h>
#include <cmath>

using namespace LV2;


class ZamComp : public Plugin<ZamComp> {
public:
  
  ZamComp(double rate)
    : Plugin<ZamComp>(8) {

    srate = rate;
    old_detected = 0.f;
    old_yl = 0.f;
    old_y1 = 0.f;

  }

  // Works on little-endian machines only
  inline bool is_nan(float& value ) {
    if (((*(uint32_t *) &value) & 0x7fffffff) > 0x7f800000) {
      return true;
    }
    return false;
  }

  // Force already-denormal float value to zero
  inline void sanitize_denormal(float& value) {
    if (is_nan(value)) {
        value = 0.f;
    }
  }

  float old_detected;  
  float old_yl; 
  float old_y1;
  float srate;

 void run(uint32_t nframes) {
  
  float attack = *p(0);
  float release = *p(1);
  float knee = *p(2);
  float ratio = *p(3);
  float threshold = exp(*p(4)/20.f*log(10.f)); 
  float makeup = exp(*p(5)/20.f*log(10.f));

  float width=(knee-0.99f)*6.f;
  float cdb=0.f;
  float attack_coeff = exp(-1000.f/(attack * srate));
  float release_coeff = exp(-1000.f/(release * srate));
  float thresdb=20.f*log10(threshold);

  float gain = 1.f;
  float xg, xl, yg, yl, y1;
  
  for (uint32_t i = 0; i < nframes; ++i) {
    yg=0.f;
    xg = (p(6)[i]==0.f) ? -160.f : 20.f*log10(fabs(p(6)[i]));
    sanitize_denormal(xg);

    if (2.f*(xg-thresdb)<-width) {
      yg = xg;
    }
    if (2.f*fabs(xg-thresdb)<=width) {
      yg = xg + (1.f/ratio-1.f)*(xg-thresdb+width/2.f)*(xg-thresdb+width/2.f)/(2.f*width);
    }
    if (2.f*(xg-thresdb)>width) {
      yg = thresdb + (xg-thresdb)/ratio;
    }
    sanitize_denormal(yg);

    xl = xg - yg;
    sanitize_denormal(old_y1);
    sanitize_denormal(old_yl);
    sanitize_denormal(old_detected);

    y1 = std::max(xl, release_coeff*old_y1+(1.f-release_coeff)*xl);
    yl = attack_coeff*old_yl+(1.f-attack_coeff)*y1;
    sanitize_denormal(y1);
    sanitize_denormal(yl);

    cdb = -yl;
    gain = exp(cdb/20.f*log(10.f));
    
    //gain reduction
    *p(8) = yl;

    p(7)[i] = p(6)[i];
    p(7)[i] *= gain * makeup;
    
    //meter_out = (fabs(p(7)[i]));
    //meter_comp = gain;
    //detected = (exp(yg/20.f*log(10.f))+old_detected)/2.f;
    //old_detected = detected;
  
    old_yl = yl; 
    old_y1 = y1;
  }
 }

};

static int _ = ZamComp::register_class("http://zamaudio.com/lv2/zamcomp");
