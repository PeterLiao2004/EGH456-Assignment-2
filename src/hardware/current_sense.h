/**
 * @file current_sense.h
 * @brief ADC current-sense interface for the DRV phase-current outputs.
 *
 * The BoosterPack exposes two phase-current sense signals to ADC-capable Tiva
 * pins:
 *   - ISENC on PE3 / ADC0 AIN0
 *   - ISENB on PD7 / ADC0 AIN4
 *
 * ISENA is not connected to an ADC-capable pin in this pinout, so phase A is
 * estimated from the balanced three-phase relationship Ia = -(Ib + Ic).
 */

#ifndef CURRENT_SENSE_H
#define CURRENT_SENSE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialise ADC0 sequence 0 for phase-current sampling.
 *
 * Configures PE3 and PD7 as ADC inputs and sets ADC0 sequence 0 to sample
 * ISENC followed by ISENB from a processor trigger.
 */
void CurrentSense_Init(void);

/**
 * @brief Read raw ADC counts from the two connected phase-current signals.
 *
 * @param phaseC_raw Receives raw ADC count for ISENC / phase C.
 * @param phaseB_raw Receives raw ADC count for ISENB / phase B.
 * @return true if a fresh conversion completed, false on timeout or bad args.
 */
bool CurrentSense_ReadRaw(uint32_t *phaseC_raw, uint32_t *phaseB_raw);

/**
 * @brief Convert the latest ADC samples into phase currents.
 *
 * @param phaseA_amps Receives estimated phase A current in amps.
 * @param phaseB_amps Receives measured phase B current in amps.
 * @param phaseC_amps Receives measured phase C current in amps.
 * @return true if a fresh conversion completed, false on timeout or bad args.
 */
bool CurrentSense_GetPhaseCurrents(float *phaseA_amps,
                                   float *phaseB_amps,
                                   float *phaseC_amps);

/**
 * @brief Estimate motor electrical power from measured phase currents.
 *
 * Uses the average absolute phase current multiplied by the configured motor
 * supply voltage. This is intended for safety-threshold monitoring rather than
 * precision power metering.
 *
 * @return Estimated motor power in watts, or 0.0f if sampling fails.
 */
float CurrentSense_GetPowerWatts(void);

#endif /* CURRENT_SENSE_H */
