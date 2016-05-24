TITLE hh.mod   squid sodium, potassium, and leak channels

: COMMENT
:  This is the original Hodgkin-Huxley treatment for the set of sodium, 
:   potassium, and leakage channels found in the squid giant axon membrane.
:   ("A quantitative description of membrane current and its application 
:   conduction and excitation in nerve" J.Physiol. (Lond.) 117:500-544 (1952).)
:  Membrane voltage is in absolute mV and has been reversed in polarity
:   from the original HH convention and shifted to reflect a resting potential
:   of -65 mV.
:  Remember to set celsius=6.3 (or whatever) in your HOC file.
:  See squid.hoc for an example of a simulation using this model.
:  SW Jaslove  6 March, 1992
: ENDCOMMENT

UNITS {
    (mA) = (milliamp)
    (mV) = (millivolt)
    (S) = (siemens)
}

NEURON {
    SUFFIX hh
    USEION na READ ena WRITE ina
    USEION k READ ek WRITE ik
    NONSPECIFIC_CURRENT il
    RANGE gnabar, gkbar, gl, el, gna, gk
    GLOBAL minf, hinf, ninf, mtau, htau, ntau
}

PARAMETER {
    gnabar = .12 (S/cm2)  : <0,1e9>
    gkbar = .036 (S/cm2)  : <0,1e9>
    gl = .0003 (S/cm2)    : <0,1e9>
    el = -54.3 (mV)
    celsius = 6.3 (degC)
    :celsius = 37 (degC)
}

STATE {
    m h n
}

ASSIGNED {
    v (mV)

    gna (S/cm2)
    gk (S/cm2)
    minf
    hinf
    ninf
    mtau (ms)
    htau (ms)
    ntau (ms)
}

BREAKPOINT {
    SOLVE states METHOD cnexp
    gna = gnabar*m*m*m*h
    ina = gna*(v - ena)
    gk = gkbar*n*n*n*n
    ik = gk*(v - ek)
    il = gl*(v - el)
}

INITIAL {
    rates(v)
    m = minf
    h = hinf
    n = ninf
}

DERIVATIVE states {
    rates(v)
    m' =  (minf-m)/mtau
    h' = (hinf-h)/htau
    n' = (ninf-n)/ntau
}

PROCEDURE rates(v) :(mV))
{
    LOCAL  alpha, beta, sum, q10

    q10 = 3^((celsius - 6.3)/10)

    :"m" sodium activation system
    alpha = .1 * vtrap(-(v+40),10)
    beta =  4 * exp(-(v+65)/18)
    sum = alpha + beta
    mtau = 1/(q10*sum)
    minf = alpha/sum

    :"h" sodium inactivation system
    alpha = .07 * exp(-(v+65)/20)
    beta = 1 / (exp(-(v+35)/10) + 1)
    sum = alpha + beta
    htau = 1/(q10*sum)
    hinf = alpha/sum

    :"n" potassium activation system
    alpha = .01*vtrap(-(v+55),10)
    beta = .125*exp(-(v+65)/80)
    sum = alpha + beta
    ntau = 1/(q10*sum)
    ninf = alpha/sum
}

FUNCTION vtrap(x,y) {
    : Traps for 0 in denominator of rate eqns.

    : Disabled for function inlining...
    : there is a very good case for making vtrap a builtin function for the language
    : - it is ubiquitous: used in so many models of ion channels
    : - platform specific optimizations written in assembly/intrinsics
    :   required to facilitate vectorization/minimize branch misses

    :if (fabs(x/y) < 1e-6) {
    :    vtrap = y*(1 - x/y/2)
    :}else{
    :    vtrap = x/(exp(x/y) - 1)
    :}

    vtrap = x/(exp(x/y) - 1)
}

