Idea: Losslessly compress data combining aspects of:
●	LFSR (linear feedback shift register)
●	Dict - a growing dict of already received symbols like in LZW
●	Literals - a bit/byte sequence of literals.  (note becomes a dict entry for reuse)
●	PRLE - patterned run length encoding.  A pattern (bit or byte or any previous entry can be repeat-emitted for a run-length. 
○	Unlike standard RLE this pattern mechanism allows any previously seen item to be used as the repeated item including previous literal-strings, PRLE sections, LFSR sequences etc
●	TXZ - compressed trie with suffix support (see TXZ)

General Implementation Method
●	Define the grammar encoding for the section types
●	Implement a decoder which implements this grammar
●	Encoders - much more difficult than decoder as it must make decisions about what the most efficient coding might be given the grammar primitives available. First swing will be a crude greedy encoder.

A key difference in this approach is the ability to combine dict & LFSR sequences to emit more powerful compression sequences rather than just relying on etnropy-like encoding of previously seen dict entries or assuming some type of locality window.


====================================

Emit sequences

#=comment

Flag-enc-num = number encoded as sequence of bytes such that top bit is terminal flag.
	Eg: 0=terminal, 1=continuing.  So 0-127 is representable by 1 byte
	2bytes = 14 bit num
	3bytes = 21 bit num
	4bytes = 28 bit num 
	5bytes = 35 bit num etc
	All nums treated as unsigned by default

Flag-enc-num-s = same as Flag-enc-num but treated as signed
	
Begin Grammar
[header]
	Magic-num  : 4 byte literal sequence
	Checksum	 : SHA-256 (of whole file except this value)
[sections] = data stored as array of the following 
	[Literal | RLE | LSFR | LZW | terminal-value, ...] 

Literal = type : len :[literals]  
#type 		= bits or bytes --> 1 bit
#len		= num literals (as flag-enc-num)
#literals	= string of literals 
*special case if len = 0 (which would imply no symbols, which doesn’t make sense) then literals is a simple number which represents a repeated section as an index of previous seen section to be repeated here.

RLE 	= run-len-type : len
		#run-len-type = bits | bytes | index-to-previous-section 
			#is 2 bit conditional field:
				#00 --> bit 0
				#01 --> bit 1
				#10 --> byte, followed by byte value
				#11 --> pattern repeat
					#00 --> literal bit pattern
						#flagencnum num-bits in pattern
						#bits[]
					#01 --> literal byte pattern
						#flagencnum num-bytes in pattern
						#bytes[]
					#10 --> index to prev existing section (this is pattrn 2 repeat)
						#encoded as flag-enc-num*
					#11 --> reserved 
		#len = flag-enc-num

*technically index to previous only needs to be as large as number of sections in bits eg if we have had 124 previous sections we only need 7 bits to represent that.  If we’re confident in our dictionary indexing, then we should just use a number which is the size log2ceil(index-length)

LSFR 	= bits : gen-poly: start : bits|bytes : len
		#bits  = #bits for LSFR as flag-enc-num
		#seed  = gen-poly (#bits wide)  #note all LSFR init’d to same strt val: 0x1
		#bits|bytes = single value determining if start & len is in bits or bytes
		#start = start pos seed in LFSR (as flag-enc-num)
		#len = #of bits or bytes (as flag-enc-num)

LZW		= section of LZW symbols.
		#note requires “accumulator” which is kept across all LZW sections


Encoder --> emits scans input binary, emits stream of data
Decoder --> extract sections and emits file

Streaming --> emit sections with optional checksum after each N sections


Use array of previous sections as literal / RLE?  Note size of nums is dependant on when these are emitted (earlier ones use less bits since they haven’t seen as many index entries)
   RLE(3):[i3,i5,i17]  #repeat section i3,i5,i7 3x
   LSFR(4):-> {i,i,i,i} #pick first 3 indexed sections from the LSFR nums 
		?what is “width” of num used to pick indexes
