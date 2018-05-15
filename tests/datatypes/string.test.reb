; datatypes/string.r
(text? "ahoj")
(not text? 1)
(text! = type of "ahoj")
; minimum
(text? "")
; alternative literal form
("" == #[text! ""])
("" == make text! 0)
("^@" = "^(00)")
("^A" = "^(01)")
("^B" = "^(02)")
("^C" = "^(03)")
("^D" = "^(04)")
("^E" = "^(05)")
("^F" = "^(06)")
("^G" = "^(07)")
("^H" = "^(08)")
("^I" = "^(09)")
("^J" = "^(0A)")
("^K" = "^(0B)")
("^L" = "^(0C)")
("^M" = "^(0D)")
("^N" = "^(0E)")
("^O" = "^(0F)")
("^P" = "^(10)")
("^Q" = "^(11)")
("^R" = "^(12)")
("^S" = "^(13)")
("^T" = "^(14)")
("^U" = "^(15)")
("^V" = "^(16)")
("^W" = "^(17)")
("^X" = "^(18)")
("^Y" = "^(19)")
("^Z" = "^(1A)")
("^[" = "^(1B)")
("^\" = "^(1C)")
("^]" = "^(1D)")
("^!" = "^(1E)")
("^_" = "^(1F)")
(" " = "^(20)")
("!" = "^(21)")
("^"" = "^(22)")
("#" = "^(23)")
("$" = "^(24)")
("%" = "^(25)")
("&" = "^(26)")
("'" = "^(27)")
("(" = "^(28)")
(")" = "^(29)")
("*" = "^(2A)")
("+" = "^(2B)")
("," = "^(2C)")
("-" = "^(2D)")
("." = "^(2E)")
("/" = "^(2F)")
("0" = "^(30)")
("1" = "^(31)")
("2" = "^(32)")
("3" = "^(33)")
("4" = "^(34)")
("5" = "^(35)")
("6" = "^(36)")
("7" = "^(37)")
("8" = "^(38)")
("9" = "^(39)")
(":" = "^(3A)")
(";" = "^(3B)")
("<" = "^(3C)")
("=" = "^(3D)")
(">" = "^(3E)")
("?" = "^(3F)")
("@" = "^(40)")
("A" = "^(41)")
("B" = "^(42)")
("C" = "^(43)")
("D" = "^(44)")
("E" = "^(45)")
("F" = "^(46)")
("G" = "^(47)")
("H" = "^(48)")
("I" = "^(49)")
("J" = "^(4A)")
("K" = "^(4B)")
("L" = "^(4C)")
("M" = "^(4D)")
("N" = "^(4E)")
("O" = "^(4F)")
("P" = "^(50)")
("Q" = "^(51)")
("R" = "^(52)")
("S" = "^(53)")
("T" = "^(54)")
("U" = "^(55)")
("V" = "^(56)")
("W" = "^(57)")
("X" = "^(58)")
("Y" = "^(59)")
("Z" = "^(5A)")
("[" = "^(5B)")
("\" = "^(5C)")
("]" = "^(5D)")
("^^" = "^(5E)")
("_" = "^(5F)")
("`" = "^(60)")
("a" = "^(61)")
("b" = "^(62)")
("c" = "^(63)")
("d" = "^(64)")
("e" = "^(65)")
("f" = "^(66)")
("g" = "^(67)")
("h" = "^(68)")
("i" = "^(69)")
("j" = "^(6A)")
("k" = "^(6B)")
("l" = "^(6C)")
("m" = "^(6D)")
("n" = "^(6E)")
("o" = "^(6F)")
("p" = "^(70)")
("q" = "^(71)")
("r" = "^(72)")
("s" = "^(73)")
("t" = "^(74)")
("u" = "^(75)")
("v" = "^(76)")
("w" = "^(77)")
("x" = "^(78)")
("y" = "^(79)")
("z" = "^(7A)")
("{" = "^(7B)")
("|" = "^(7C)")
("}" = "^(7D)")
("~" = "^(7E)")
("^~" = "^(7F)")
("^(null)" = "^(00)")
("^(line)" = "^(0A)")
("^/" = "^(0A)")
("^(tab)" = "^(09)")
("^-" = "^(09)")
("^(page)" = "^(0C)")
("^(esc)" = "^(1B)")
("^(back)" = "^(08)")
("^(del)" = "^(7f)")
("ahoj" = #[text! "ahoj"])
("1" = to text! 1)
({""} = mold "")


[#854 (
    a: <0>
    b: make tag! 0
    insert b first a
    a == b
)]
