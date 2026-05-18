/* opt3001.h */

#ifndef OPT3001_H
#define OPT3001_H

#ifdef __cplusplus
extern "C"
{
#endif


/*********************************************************************
 * FUNCTIONS
 */
extern bool sensorOpt3001Init(void);
extern bool sensorOpt3001Enable(bool enable);
extern bool sensorOpt3001Read(uint16_t *rawData);
extern void sensorOpt3001Convert(uint16_t rawData, float *convertedLux);
extern bool sensorOpt3001Test(void);

#ifdef __cplusplus
}
#endif

#endif /* OPT3001_H */
