; MOD is the tight infix form of MODULO

(zero? 1E15 mod 1)
(zero? -1E15 mod 1)
(zero? 1E14 mod 1)
(zero? -1E14 mod 1)
(zero? -1 mod 1)
(0.75 == -1.25 mod 1)
(0.5 == -1.5 mod 1)
(0.25 == -1.75 mod 1)
(-0.25 == -1.25 mod -1)
(-0.5 == -1.5 mod -1)
(-0.75 == -1.75 mod -1)
; these have small error; due to binary approximation of decimal numbers
(not negative? 1e-8 - abs 0.9 - (99'999'999.9 mod 1))
(not negative? 1e-8 - abs 0.99 - (99'999'999.99 mod 1))
(not negative? 1e-8 - abs 0.999 - (99'999'999.999 mod 1))
(not negative? 1e-8 - abs 0.9999 - (99'999'999.9999 mod 1))
(not negative? 1e-8 - abs 0.99999 - (99'999'999.99999 mod 1))
(not negative? 1e-8 - abs 0.999999 - (99'999'999.999999 mod 1))
(zero? $999'999'999'999'999 mod 1)
(zero? $999'999'999'999'999 mod $1)
(zero? 9'999'999'999'999'999 mod 1.0)
(zero? 999'999'999'999'999 mod 1.0)
(zero? 562'949'953'421'311.0 mod 1)
(zero? -562'949'953'421'311.0 mod 1)
(0.25 == 562'949'953'421'311.25 mod 1)
(zero? modulo/adjusted 562'949'953'421'311.25 1)
(0.5 == 562'949'953'421'311.5 mod 1)
(0.5 == -562'949'953'421'311.5 mod 1)
(0.25 == -562'949'953'421'311.75 mod 1)
(zero? 562'949'953'421'312.0 mod 1)
(zero? -562'949'953'421'312.0 mod 1)
(0.25 == 562'949'953'421'312.25 mod 1)
(0.5 == -562'949'953'421'312.5 mod 1)
(0.5 == 562'949'953'421'312.5 mod 1)
(0.25 == -562'949'953'421'312.75 mod 1)
(zero? 562'949'953'421'313.0 mod 1.0)
(zero? -562'949'953'421'313.0 mod 1.0)
(0.5 == -562'949'953'421'313.5 mod 1)
(0.5 == 562'949'953'421'313.5 mod 1)
(zero? -562'949'953'421'314.0 mod 1)
(0.5 == -562'949'953'421'314.5 mod 1)
(0.5 == 562'949'953'421'314.5 mod 1)
(zero? modulo/adjusted 0.15 - 0.05 - 0.1 0.1)
(zero? modulo/adjusted 0.1 + 0.1 + 0.1 0.3)
(zero? modulo/adjusted 0.3 0.1 + 0.1 + 0.1)
(zero? modulo/adjusted 0.1 + 0.1 + 0.1 0.3)
(zero? modulo/adjusted 0.3 0.1 + 0.1 + 0.1)
(zero? modulo/adjusted $0.1 + $0.1 + $0.1 $0.3)
(zero? modulo/adjusted $0.3 $0.1 + $0.1 + $0.1)
(zero? modulo/adjusted 1 0.1)
(zero? modulo/adjusted 0.15 - 0.05 - 0.1 0.1)
[#56
    (zero? modulo/adjusted 1 1)
]
