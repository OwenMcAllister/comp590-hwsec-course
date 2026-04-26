# Rowhammer Lab Discussion Questions

## 1-2 Discussion Question
### In a 64-bit system using 4KB pages, which bits are used to represent the page offset, and which are used to represent the page number?
Pages are 4KB = 2^12 bytes. Thus, they're bits 11-0 of the virtual address. The remaining 52 bits, 12-63, are the page number.

### How about for a 64-bit system using 2MB pages? Which bits are used for page number and which are for page offset?
Pages are 2MB = 2^21 bytes. Thus, you need 21 bits for the page offset, 20-0, and bits 47-21 are for the page number.

### In a 2GB buffer, how many 2MB hugepages are there?
2GB = 2^31
2MB = 2^21

2^31 / 2^21 = 2^10 = 1024 hugepages

## 2-1 Discussion Question
### Given a victim address 0x96ec3000, what is the value of its <Row id>? The value of its <Column id>?
0x96ec3000 = 1001 0110 1110 1100 0011 0000 0000 0000

Row ID = 1001 0110 1110 110 = 0x4B76
Column ID = 1 0000 0000 0000 = 0x1000

### For this same victim address, when the exact XOR function being used for computing the <Bank id>is unknown, list all possible attacker addresses that stays in the row below the victim address (i.e., the attacker’s <Row id>is 1 more than the victim’s while sharing the same <Column id>and <Bank id>. Hint: there should be 16 such addresses total.

Row ID = 0x4B76 + 1 = 0x4B77 = 1001 0110 1110 111
Column ID = 1 0000 0000 0000
Bank ID = ?

1. 1001 0110 1110 1110 0001 0000 0000 0000
2. 1001 0110 1110 1110 0011 0000 0000 0000
3. 1001 0110 1110 1110 0101 0000 0000 0000
4. 1001 0110 1110 1110 0111 0000 0000 0000
5. 1001 0110 1110 1110 1001 0000 0000 0000
6. 1001 0110 1110 1110 1011 0000 0000 0000
7. 1001 0110 1110 1110 1101 0000 0000 0000
8. 1001 0110 1110 1110 1111 0000 0000 0000
9. 1001 0110 1110 1111 0001 0000 0000 0000
10. 1001 0110 1110 1111 0011 0000 0000 0000
11. 1001 0110 1110 1111 0101 0000 0000 0000
12. 1001 0110 1110 1111 0111 0000 0000 0000
13. 1001 0110 1110 1111 1001 0000 0000 0000
14. 1001 0110 1110 1111 1011 0000 0000 0000
15. 1001 0110 1110 1111 1101 0000 0000 0000
16. 1001 0110 1110 1111 1111 0000 0000 0000

## 2-3 Discussion Question
### Analyze the statistics produced by your code when running part2, and report a threshold to distinguish the bank conflict. Note that you can toggle the number of SAMPLES.

Threshold: 288

```
 248 : 1      #
 252 : 1      #
 256 : 3      #
 260 : 8      ###
 264 : 25     #########
 268 : 106    ########################################
 272 : 159    ############################################################
 276 : 56     #####################
 280 : 52     ###################
 284 : 1      #
 288 : 4      #
 292 : 1      #
 296 : 2      #
 300 : 2      #
 308 : 1      #
 312 : 5      #
 316 : 1      #
 320 : 1      #
 324 : 3      #
 328 : 2      #
 332 : 6      ##
 336 : 9      ###
 340 : 7      ##
 344 : 10     ###
 348 : 5      #
 352 : 9      ###
 356 : 7      ##
 360 : 8      ###
 364 : 4      #
 368 : 4      #
 376 : 2      #
 492 : 17     ######
 496 : 14     #####
 500 : 19     #######
 504 : 37     #############
 508 : 21     #######
 512 : 4      #
 516 : 3      #
 572 : 1      #
 584 : 103    ######################################
 588 : 87     ################################
 592 : 40     ###############
 596 : 80     ##############################
 600 : 14     #####
 604 : 5      #
 676 : 6      ##
 680 : 6      ##
 684 : 16     ######
 688 : 1      #
 692 : 15     #####
 696 : 6      ##
 ```

## 3-2 Discussion Question
### Based on the XOR function you reverse-engineered, determine which of the 16 candidate addresses you derived in Discussion Question 2-1 maps to the same bank.

Function:
```
F0: {A14 ^ A17, A15 ^ A18, A16 ^ A19, A7 ^ A8 ^ A9 ^ A12 ^ A13 ^ A15 ^ A16}
```

Victim address: 1001 0110 1110 1100 0011 0000 0000 0000
A12 = 1
A13 = 1
A14 = 0
A15 = 0
A16 = 0
A17 = 0
A18 = 1
A19 = 1

F0 = {0, 1, 1, 0}

6. 1001 0110 1110 1110 1011 0000 0000 0000
A12 = 1
A13 = 1
A14 = 0
A15 = 1
A16 = 0
A17 = 1
A18 = 1
A19 = 1

F0 = {0, 1, 1, 0}

## 4-2 Discussion Question
### The default data pattern in part4.cc is to set aggressor rows to all 1’s and victim row to all 0’s. Try different data pattern and include the bitflip observation statistics in the table below. Then answer the following questions: Do your results match your expectations? What is the best pattern to trigger flips effectively?

|Data Pattern (Victim/Aggressor)   | 0x00/0xff  |0xff/0x00   |0x00/0x00    |0xff/0xff   |
|---|---|---|---|---|
| Number of Flips (100 trials)  |   |   |   |   |

## 5-1 Discussion Question
### Given the ECC type descriptions listed above, fill in the following table (assuming a data length of 4). For detection/correction, answer “X” only if it can always detect/correct X number of errors, with no corner case exceptions. For detecting more than 1 error, the scheme is not required to tell exactly how many errors exist. We’ve filled in the first column for you.

| Metric | 1-Repetition (No ECC) | 2-Repetition | 3-Repetition | Single Parity Bit | Hamming(7,4) |
|---|---|---|---|---|---|
| Code Rate (Data Bits / Total Bits) | 1.0 |  |  |  |  |
| Max Number of Errors Can Detect | 0 |  |  |  |  |
| Max Number of Errors Can Correct | 0 |  |  |  |  |

## 5-3 Discussion Question
### When a single bit flip is detected, describe what action should be conducted to correct this error with Hamming(22,16).

## 5-5 Discussion Question
### Can the Hamming(22,16) code we implemented always protect us from rowhammer attacks? If not, describe how a clever attacker could work around this scheme.