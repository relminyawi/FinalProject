//modified from http://chipkit.net/forum/viewtopic.php?f=15&t=512&p=2633

#define LOG2_N_WAVE 11                 // 11 bits
#define N_WAVE (1 << LOG2_N_WAVE)      // 2048 dots
#define N_WAVE34 (N_WAVE - N_WAVE / 4) // Sinus lookup table size (3/4 wave)

#define OVERSAMPLING 2                 // 0 for 1x
                                       // 1 for 2x
                                       // 2 for 4x
int16_t Sinewave[N_WAVE34];

int16_t samples[N_WAVE];
volatile bool samplesReady = false;
volatile uint16_t sampleCounter = 0;

 
 
 // ADC setup
 AD1PCFG = ~1;          // PORTB digital, RB0 analog
 AD1CHSbits.CH0SA = 0;  // Channel 0 positive input is AN0
 //AD1CON2bits.VCFG = 1;  // External VREF+ pin
 AD1CON1bits.SSRC = 7;  // Internal counter ends sampling and starts conversion (auto-convert)
 AD1CON1bits.ASAM = 1;  // Sampling begins when SAMP bit is set

 // ADC Conversion Clock Select
 // TPB = (1 / 80MHz) * 2 = 25ns
 // TAD = TPB * (ADCS + 1) * 2 = 25ns * 20 = 500ns
 AD1CON3bits.ADCS = 19;

 // Auto-Sample Time
 // SAMC * TAD = 30 * 500ns = 15uS
 AD1CON3bits.SAMC = 30;

 IPC6bits.AD1IP = 6;    // Analog-to-Digital 1 Interrupt Priority
 IPC6bits.AD1IS = 3;    // Analog-to-Digital 1 Subpriority
 IFS1CLR = 2;           // Clear ADC interrupt flag
 IEC1bits.AD1IE = 1;    // ADC interrupt enable
 AD1CON1bits.ON = 1;    // ADC enable
 
 void main(){
 
  static uint16_t i;
  static uint8_t j;
  static int16_t real[N_WAVE];
  static int16_t imag[N_WAVE];
  static uint16_t amplitude[N_WAVE / 2];

 
  while(!samplesReady);
  samplesReady = false;

  for(i = 0; i < N_WAVE >> OVERSAMPLING; i++) {
	real[i] = samples[i << OVERSAMPLING];
	for(j = 1; j < 1 << OVERSAMPLING; j++) {
		real[i] += samples[(i << OVERSAMPLING) + j];
	}
	real[i] = (real[i] >> OVERSAMPLING) * GAIN;
	imag[i] = 0;
  }
  AD1CON1bits.ASAM = 1; // Start autosampling

  fix_fft(real, imag, LOG2_N_WAVE, 0);

  for(i = OFFSETX; i < OFFSETX + TFTX; i++)
	amplitude[i] = sqrt(real[i] * real[i] + imag[i] * imag[i]);

  }
 
 
 void __ISR(_ADC_VECTOR, ipl6) ADCInterruptHandler() {
  IFS1CLR = 2; // Clear ADC interrupt flag

  samples[sampleCounter++] = ADC1BUF0 - 512;

  if(sampleCounter >= N_WAVE) {
   sampleCounter = 0;
   samplesReady = true;
   AD1CON1bits.ASAM = 0; // Stop autosampling
  }
 }
/*
  FIX_MPY() - fixed-point multiplication & scaling.
  Substitute inline assembly for hardware-specific
  optimization suited to a particluar DSP processor.
  Scaling ensures that result remains 16-bit.
*/
inline short FIX_MPY(short a, short b)
{
  /* shift right one less bit (i.e. 15-1) */
  int c = ((int)a * (int)b) >> 14;
  /* last bit shifted out = rounding-bit */
  b = c & 0x01;
  /* last shift + rounding bit */
  a = (c >> 1) + b;
  return a;
} 
 
/*
  fix_fft() - perform forward/inverse fast Fourier transform.
  fr[n],fi[n] are real and imaginary arrays, both INPUT AND
  RESULT (in-place FFT), with 0 <= n < 2**m; set inverse to
  0 for forward transform (FFT), or 1 for iFFT.
*/
int fix_fft(short fr[], short fi[], short m, short inverse)
{
  int mr, nn, i, j, l, k, istep, n, scale, shift;
  short qr, qi, tr, ti, wr, wi;

  n = 1 << m;

  /* max FFT size = N_WAVE */
  if (n > N_WAVE)
    return -1;

  mr = 0;
  nn = n - 1;
  scale = 0;

  /* decimation in time - re-order data */
  for (m=1; m<=nn; ++m) {
    l = n;
    do {
      l >>= 1;
    } while (mr+l > nn);
    mr = (mr & (l-1)) + l;

    if (mr <= m)
      continue;
    tr = fr[m];
    fr[m] = fr[mr];
    fr[mr] = tr;
    ti = fi[m];
    fi[m] = fi[mr];
    fi[mr] = ti;
  }

  l = 1;
  k = LOG2_N_WAVE-1;
  while (l < n) {
    if (inverse) {
      /* variable scaling, depending upon data */
      shift = 0;
      for (i=0; i<n; ++i) {
        j = fr[i];
        if (j < 0)
          j = -j;
        m = fi[i];
        if (m < 0)
          m = -m;
        if (j > 16383 || m > 16383) {
          shift = 1;
          break;
        }
      }
      if (shift)
        ++scale;
    } else {
      /*
        fixed scaling, for proper normalization --
        there will be log2(n) passes, so this results
        in an overall factor of 1/n, distributed to
        maximize arithmetic accuracy.
      */
      shift = 1;
    }
    /*
      it may not be obvious, but the shift will be
      performed on each data point exactly once,
      during this pass.
    */
    istep = l << 1;
    for (m=0; m<l; ++m) {
      j = m << k;
      /* 0 <= j < N_WAVE/2 */
      wr =  Sinewave[j+N_WAVE/4];
      wi = -Sinewave[j];
      if (inverse)
        wi = -wi;
      if (shift) {
        wr >>= 1;
        wi >>= 1;
      }
      for (i=m; i<n; i+=istep) {
        j = i + l;
        tr = FIX_MPY(wr,fr[j]) - FIX_MPY(wi,fi[j]);
        ti = FIX_MPY(wr,fi[j]) + FIX_MPY(wi,fr[j]);
        qr = fr[i];
        qi = fi[i];
        if (shift) {
          qr >>= 1;
          qi >>= 1;
        }
        fr[j] = qr - tr;
        fi[j] = qi - ti;
        fr[i] = qr + tr;
        fi[i] = qi + ti;
      }
    }
    --k;
    l = istep;
  }
  return scale;
}
