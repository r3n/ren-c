; functions/file/open.r
[#1422 (  ; "Rebol crashes when opening the 128th port"
    repeat n 200 [
        close open open join tcp://localhost: n
    ]
    true
)]
