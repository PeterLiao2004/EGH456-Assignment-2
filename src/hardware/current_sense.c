/**
 * @file current_sense.c
 * @brief ADC current-sense implementation.
 */

#include "hardware/current_sense.h"

#include <stddef.h>

#include "inc/hw_memmap.h"
#include "driverlib/adc.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"

/** @brief ADC full-scale count for the 12-bit Tiva ADC. */
#define CURRENT_SENSE_ADC_MAX          4095.0f

/** @brief ADC reference voltage in volts. */
#define CURRENT_SENSE_ADC_VREF         3.3f

/** @brief Current-sense amplifier gain. Check against the board configuration. */
#ifndef CURRENT_SENSE_CSA_GAIN
#define CURRENT_SENSE_CSA_GAIN         20.0f
#endif

/** @brief Phase-current shunt resistance in ohms. Check the board revision. */
#ifndef CURRENT_SENSE_SHUNT_OHMS
#define CURRENT_SENSE_SHUNT_OHMS       0.007f
#endif

/** @brief Bidirectional current-sense midpoint voltage in volts. */
#ifndef CURRENT_SENSE_CSA_VREF
#define CURRENT_SENSE_CSA_VREF         1.65f
#endif

/** @brief Motor supply voltage used for approximate power estimation. */
#ifndef CURRENT_SENSE_MOTOR_VOLTAGE
#define CURRENT_SENSE_MOTOR_VOLTAGE    24.0f
#endif

/** @brief ADC0 sequencer used for the two current-sense channels. */
#define CURRENT_SENSE_ADC_SEQUENCE     0U

/** @brief Poll limit used to avoid a permanent stall if ADC conversion fails. */
#define CURRENT_SENSE_ADC_TIMEOUT      100000U

static bool s_currentSenseInitialised = false;

static float prvAbsFloat(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float prvRawToPhaseCurrent(uint32_t raw)
{
    float voltage = ((float)raw / CURRENT_SENSE_ADC_MAX) * CURRENT_SENSE_ADC_VREF;
    return (voltage - CURRENT_SENSE_CSA_VREF) /
           (CURRENT_SENSE_CSA_GAIN * CURRENT_SENSE_SHUNT_OHMS);
}

void CurrentSense_Init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);

    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0))
    {
    }
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOE))
    {
    }
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD))
    {
    }

    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);  /* PE3 = AIN0 = ISENC */
    GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_7);  /* PD7 = AIN4 = ISENB */

    ADCSequenceDisable(ADC0_BASE, CURRENT_SENSE_ADC_SEQUENCE);
    ADCSequenceConfigure(ADC0_BASE,
                         CURRENT_SENSE_ADC_SEQUENCE,
                         ADC_TRIGGER_PROCESSOR,
                         0);

    ADCSequenceStepConfigure(ADC0_BASE,
                             CURRENT_SENSE_ADC_SEQUENCE,
                             0,
                             ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC0_BASE,
                             CURRENT_SENSE_ADC_SEQUENCE,
                             1,
                             ADC_CTL_CH4 | ADC_CTL_IE | ADC_CTL_END);

    ADCSequenceEnable(ADC0_BASE, CURRENT_SENSE_ADC_SEQUENCE);
    ADCIntClear(ADC0_BASE, CURRENT_SENSE_ADC_SEQUENCE);

    s_currentSenseInitialised = true;
}

bool CurrentSense_ReadRaw(uint32_t *phaseC_raw, uint32_t *phaseB_raw)
{
    uint32_t adcValues[8];
    uint32_t timeout = CURRENT_SENSE_ADC_TIMEOUT;

    if ((phaseC_raw == NULL) || (phaseB_raw == NULL))
    {
        return false;
    }

    if (!s_currentSenseInitialised)
    {
        CurrentSense_Init();
    }

    (void)ADCSequenceDataGet(ADC0_BASE, CURRENT_SENSE_ADC_SEQUENCE, adcValues);
    ADCIntClear(ADC0_BASE, CURRENT_SENSE_ADC_SEQUENCE);
    ADCProcessorTrigger(ADC0_BASE, CURRENT_SENSE_ADC_SEQUENCE);

    while ((ADCIntStatus(ADC0_BASE, CURRENT_SENSE_ADC_SEQUENCE, false) == 0U) &&
           (timeout > 0U))
    {
        timeout--;
    }

    if (timeout == 0U)
    {
        return false;
    }

    ADCIntClear(ADC0_BASE, CURRENT_SENSE_ADC_SEQUENCE);
    if (ADCSequenceDataGet(ADC0_BASE, CURRENT_SENSE_ADC_SEQUENCE, adcValues) < 2)
    {
        return false;
    }

    *phaseC_raw = adcValues[0];  /* AIN0 = ISENC */
    *phaseB_raw = adcValues[1];  /* AIN4 = ISENB */

    return true;
}

bool CurrentSense_GetPhaseCurrents(float *phaseA_amps,
                                   float *phaseB_amps,
                                   float *phaseC_amps)
{
    uint32_t rawC;
    uint32_t rawB;
    float currentB;
    float currentC;

    if ((phaseA_amps == NULL) ||
        (phaseB_amps == NULL) ||
        (phaseC_amps == NULL))
    {
        return false;
    }

    if (!CurrentSense_ReadRaw(&rawC, &rawB))
    {
        return false;
    }

    currentC = prvRawToPhaseCurrent(rawC);
    currentB = prvRawToPhaseCurrent(rawB);

    *phaseC_amps = currentC;
    *phaseB_amps = currentB;
    *phaseA_amps = -(currentB + currentC);

    return true;
}

float CurrentSense_GetPowerWatts(void)
{
    float currentA;
    float currentB;
    float currentC;
    float averageCurrent;

    if (!CurrentSense_GetPhaseCurrents(&currentA, &currentB, &currentC))
    {
        return 0.0f;
    }

    averageCurrent = (prvAbsFloat(currentA) +
                      prvAbsFloat(currentB) +
                      prvAbsFloat(currentC)) / 3.0f;

    return CURRENT_SENSE_MOTOR_VOLTAGE * averageCurrent;
}
