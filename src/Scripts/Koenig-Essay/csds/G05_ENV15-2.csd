<CsoundSynthesizer>

; Id: G05_ENV15-2.CSD mg (2006, rev.2009)
; author: marco gasperini (marcogsp at yahoo dot it)

; G.M. Koenig
; ESSAY (1957)

<CsOptions>
-W -f -oG05_ENV15-2.wav
</CsOptions>

<CsInstruments>

sr     = 192000
kr     = 192000
ksmps  = 1
nchnls = 1

;=============================================
; 275.15 INTENSITY CURVES
;=============================================
	instr 1
iamp	= ampdb(p4)

idur	= p3
ifile	= p5

a1	diskin2  ifile, 1

aout	=  a1*iamp 

	out aout
	endin
;=============================================

</CsInstruments>
<CsScore>
t0	4572		; 76.2 cm/sec. tape speed (durations in cm)	

;			p4	p5	
;			iamp1	ifile
;			[dB]	
i1	0	1503	-5	"G04_REV15-2.wav"

e

</CsScore>
</CsoundSynthesizer>