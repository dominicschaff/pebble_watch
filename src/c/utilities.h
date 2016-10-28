#define M_PI 3.141592653589793
float my_sqrt(const float x);
float my_floor(float x); 
float my_fabs(float x);
float my_atan(float x);
float my_rint (float x);
float my_sin (float x);
float my_cos(float x);
float my_acos (float x);
float my_asin (float x);
float my_tan(float x);

#define ZENITH_OFFICIAL 90.83
#define ZENITH_CIVIL    96.0
#define ZENITH_NAUTICAL 102.0
#define ZENITH_ASTRONOMICAL 108.0

float calcSun(int year, int month, int day, float latitude, float longitude, int sunset, float zenith);
float calcSunRise(int year, int month, int day, float latitude, float longitude, float zenith);
float calcSunSet(int year, int month, int day, float latitude, float longitude, float zenith);


double atof(const char *nptr);

void format_number(char *str, int size, int number);

double round(double number);