; (CRC32 is in CHECKSUM-CORE too, but test it in Crypt)

(#{4F57A50D} = checksum-core 'crc32 "More tests needed")
(#{4F57A50D} = checksum 'crc32 "More tests needed")

(#{2165738C} = checksum/method to-binary "foo" 'CRC32)
(#{00000000} = checksum/method to-binary "" 'CRC32)
