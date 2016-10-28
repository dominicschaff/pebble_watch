/*
 * loosely based on 
 * - http://stackoverflow.com/questions/11261170/c-and-maths-fast-approximation-of-a-trigonometric-function
 * - http://www.codeproject.com/Articles/69941/Best-Square-Root-Method-Algorithm-Function-Precisi
 */
#include <pebble.h>
#include "utilities.h"

#define SQRT_MAGIC_F 0x5f3759df 
float my_sqrt(const float x)
{
  const float xhalf = 0.5f*x;
 
  union // get bits for floating value
  {
    float x;
    int i;
  } u;
  u.x = x;
  u.i = SQRT_MAGIC_F - (u.i >> 1);  // gives initial guess y0
  return x*u.x*(1.5f - xhalf*u.x*u.x);// Newton step, repeating increases accuracy 
}   

float my_floor(float x) 
{
  return ((int)x);
}

float my_fabs(float x)
{
  if (x<0) return -x;
  return x;
}

float my_atan(float x)
{
  if (x>0) 
  {
    return (M_PI/2)*(0.596227*x + x*x)/(1 + 2*0.596227*x + x*x);
  } 
  else
  {
    return -(my_atan(-x));
  }
}

/* not quite rint(), i.e. results not properly rounded to nearest-or-even */
float my_rint (float x)
{
  float t = my_floor (my_fabs(x) + 0.5);
  return (x < 0.0) ? -t : t;
}

/* minimax approximation to cos on [-pi/4, pi/4] with rel. err. ~= 7.5e-13 */
float cos_core (float x)
{
  float x8, x4, x2;
  x2 = x * x;
  x4 = x2 * x2;
  x8 = x4 * x4;
  /* evaluate polynomial using Estrin's scheme */
  return (-2.7236370439787708e-7 * x2 + 2.4799852696610628e-5) * x8 +
         (-1.3888885054799695e-3 * x2 + 4.1666666636943683e-2) * x4 +
         (-4.9999999999963024e-1 * x2 + 1.0000000000000000e+0);
}

/* minimax approximation to sin on [-pi/4, pi/4] with rel. err. ~= 5.5e-12 */
float sin_core (float x)
{
  float x4, x2;
  x2 = x * x;
  x4 = x2 * x2;
  /* evaluate polynomial using a mix of Estrin's and Horner's scheme */
  return ((2.7181216275479732e-6 * x2 - 1.9839312269456257e-4) * x4 + 
          (8.3333293048425631e-3 * x2 - 1.6666666640797048e-1)) * x2 * x + x;
}

/* minimax approximation to arcsin on [0, 0.5625] with rel. err. ~= 1.5e-11 */
float asin_core (float x)
{
  float x8, x4, x2;
  x2 = x * x;
  x4 = x2 * x2;
  x8 = x4 * x4;
  /* evaluate polynomial using a mix of Estrin's and Horner's scheme */
  return (((4.5334220547132049e-2 * x2 - 1.1226216762576600e-2) * x4 +
           (2.6334281471361822e-2 * x2 + 2.0596336163223834e-2)) * x8 +
          (3.0582043602875735e-2 * x2 + 4.4630538556294605e-2) * x4 +
          (7.5000364034134126e-2 * x2 + 1.6666666300567365e-1)) * x2 * x + x; 
}

/* relative error < 7e-12 on [-50000, 50000] */
float my_sin (float x)
{
  float q, t;
  int quadrant;
  /* Cody-Waite style argument reduction */
  q = my_rint (x * 6.3661977236758138e-1);
  quadrant = (int)q;
  t = x - q * 1.5707963267923333e+00;
  t = t - q * 2.5633441515945189e-12;
  if (quadrant & 1) {
    t = cos_core(t);
  } else {
    t = sin_core(t);
  }
  return (quadrant & 2) ? -t : t;
}

float my_cos(float x)
{
  return my_sin(x + (M_PI/2));
}

/* relative error < 2e-11 on [-1, 1] */
float my_acos (float x)
{
  float xa, t;
  xa = my_fabs (x);
  /* arcsin(x) = pi/2 - 2 * arcsin (sqrt ((1-x) / 2)) 
   * arccos(x) = pi/2 - arcsin(x)
   * arccos(x) = 2 * arcsin (sqrt ((1-x) / 2))
   */
  if (xa > 0.5625) {
    t = 2.0 * asin_core (my_sqrt (0.5 * (1.0 - xa)));
  } else {
    t = 1.5707963267948966 - asin_core (xa);
  }
  /* arccos (-x) = pi - arccos(x) */
  return (x < 0.0) ? (3.1415926535897932 - t) : t;
}

float my_asin (float x)
{
  return (M_PI/2) - my_acos(x);
}

float my_tan(float x)
{
  return my_sin(x) / my_cos(x);
}

float calcSun(int year, int month, int day, float latitude, float longitude, int sunset, float zenith)
{
  int N1 = my_floor(275 * month / 9);
  int N2 = my_floor((month + 9) / 12);
  int N3 = (1 + my_floor((year - 4 * my_floor(year / 4) + 2) / 3));
  int N = N1 - (N2 * N3) + day - 30;

  float lngHour = longitude / 15;
  
  float t;
  if (!sunset)
  {
    //if rising time is desired:
    t = N + ((6 - lngHour) / 24);
  }
  else
  {
    //if setting time is desired:
    t = N + ((18 - lngHour) / 24);
  }

  float M = (0.9856 * t) - 3.289;

  //calculate the Sun's true longitude
  //L = M + (1.916 * sin(M)) + (0.020 * sin(2 * M)) + 282.634
  float L = M + (1.916 * my_sin((M_PI/180.0f) * M)) + (0.020 * my_sin((M_PI/180.0f) * 2 * M)) + 282.634;
  if (L<0) L+=360.0f;
  if (L>360) L-=360.0f;

  //5a. calculate the Sun's right ascension
  //RA = atan(0.91764 * tan(L))
  float RA = (180.0f/M_PI) * my_atan(0.91764 * my_tan((M_PI/180.0f) * L));
  if (RA<0) RA+=360;
  if (RA>360) RA-=360;

  //5b. right ascension value needs to be in the same quadrant as L
  float Lquadrant  = (my_floor( L/90)) * 90;
  float RAquadrant = (my_floor(RA/90)) * 90;
  RA = RA + (Lquadrant - RAquadrant);

  //5c. right ascension value needs to be converted into hours
  RA = RA / 15;

  //6. calculate the Sun's declination
  float sinDec = 0.39782 * my_sin((M_PI/180.0f) * L);
  float cosDec = my_cos(my_asin(sinDec));

  //7a. calculate the Sun's local hour angle
  //cosH = (cos(zenith) - (sinDec * sin(latitude))) / (cosDec * cos(latitude))
  float cosH = (my_cos((M_PI/180.0f) * zenith) - (sinDec * my_sin((M_PI/180.0f) * latitude))) / (cosDec * my_cos((M_PI/180.0f) * latitude));
  
  if (cosH >  1) {
    return 0;
  }
  else if (cosH < -1)
  {
    return 0;
  }
    
  //7b. finish calculating H and convert into hours
  
  float H;
  if (!sunset)
  {
    //if rising time is desired:
    H = 360 - (180.0f/M_PI) * my_acos(cosH);
  }
  else
  {
    //if setting time is desired:
    H = (180.0f/M_PI) * my_acos(cosH);
  }
  
  H = H / 15;

  //8. calculate local mean time of rising/setting
  float T = H + RA - (0.06571 * t) - 6.622;

  //9. adjust back to UTC
  float UT = T - lngHour;
  if (UT<0) {UT+=24;}
  if (UT>24) {UT-=24;}

  return UT;
}

float calcSunRise(int year, int month, int day, float latitude, float longitude, float zenith)
{
  return calcSun(year, month, day, latitude, longitude, 0, zenith);
}

float calcSunSet(int year, int month, int day, float latitude, float longitude, float zenith)
{
  return calcSun(year, month, day, latitude, longitude, 1, zenith);
}

int isspace(int c)
{
  if(((char)c)==' ')
    return 1;
  return 0;
}

int isdigit(int c)
{
  char cc = (char)c;
  if(cc>='0' && cc<='9')
    return 1;
  return 0;
}

double strtod(const char *nptr, char **endptr)
{
    double x = 0.0;
    double xs= 1.0;
    double es = 1.0;
    double xf = 0.0;
    double xd = 1.0;
    while( isspace( (unsigned char)*nptr ) ) ++nptr;
    if(*nptr == '-')
    {
        xs = -1;
        nptr++;
    }
    else if(*nptr == '+')
    {
        nptr++;
    }

    while (1)
    {
        if (isdigit((unsigned char)*nptr))
        {
            x = x * 10 + (*nptr - '0');
            nptr++;
        }
        else
        {
            x = x * xs;
            break;
        }
    }
    if (*nptr == '.')
    {
        nptr++;
        while (1)
        {
            if (isdigit((unsigned char)*nptr))
            {
                xf = xf * 10 + (*nptr - '0');
                xd = xd * 10;
            }
            else
            {
                x = x + xs * (xf / xd);
                break;
            }
            nptr++;
        }
    }
    if ((*nptr == 'e') || (*nptr == 'E'))
    {
        nptr++;
        if (*nptr == '-')
        {
            es = -1;
            nptr++;
        }
        xd = 1;
        xf = 0;
        while (1)
        {
            if (isdigit((unsigned char)*nptr))
            {
                xf = xf * 10 + (*nptr - '0');
                nptr++;
            }
            else
            {
                while (xf > 0)
                {
                    xd *= 10;
                    xf--;
                }
                if (es < 0.0)
                {
                    x = x / xd;
                }
                else
                {
                    x = x * xd;
                }
                break;
            }
        }
    }
    if (endptr != NULL)
    {
        *endptr = (char *)nptr;
    }
    return (x);
}

double atof(const char *nptr)
{
    return (strtod(nptr, (char **)NULL));
}


void format_number(char *str, int size, int number)
{
  if (number > 1000)
    snprintf(str, size, "%d,%03d", number / 1000, number % 1000);
  else
    snprintf(str, size, "%d", number);
}

double round(double number)
{
    return (number >= 0) ? (int)(number + 0.5) : (int)(number - 0.5);
}