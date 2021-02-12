; datatypes/date.r

(date? 25-Sep-2006)
(path? '25/Sep/2006)  ; This was a DATE in R3-Alpha, but now it's a PATH!

(not date? 1)
(date! = type of 25-Sep-2006)

; alternative formats
(25-Sep-2006 = 25-9-2006)
(25-Sep-2006 = make date! "25/Sep/2006")
(25-Sep-2006 = to date! "25-Sep-2006")
("25-Sep-2006" = mold 25-Sep-2006)

; minimum
(date? 1-Jan-0000)

; another minimum
(date? 1-Jan-0000/0:00)

; extreme behaviour
(
    did any [
        error? trap [date-d: 1-Jan-0000 - 1]
        date-d = load-value mold date-d
    ]
)
(
    did any [
        error? trap [date-d: 31-Dec-16383 + 1]
        date-d = load-value mold date-d
    ]
)

[#1250 (
    did all [
        error? trap [load "1/11--00"]
        error? trap [load "1/11--0"]
        (load-value "1-11-0") = (load-value "1-11-00")
    ]
)]

[#213 (
    d: 28-Mar-2019/17:25:40-4:00
    d: d/date
    (d + 1) == 29-Mar-2019
)]

[https://github.com/red/red/issues/3881 (
    d: 29-Feb-2020
    d/year: d/year - 1
    d = 1-Mar-2019
)]

[#1637 (
    d: now/date
    did all [
        null? :d/time
        null? :d/zone
    ]
)]

(
    [d n]: transcode "1975-04-21/10:20:03.04"
    did all [
        date? d
        n = ""
        d/year = 1975
        d/month = 4
        d/day = 21
        d/hour = 10
        d/minute = 20
        d/second = 3.04
    ]
)

(2020-11-24 < 2020-11-25)
(not 2020-11-24 > 2020-11-25)
