## 1-2

**In a 64-bit system using 4KB pages, which bits are used to represent the page offset, and which are used to represent the page number?**
Page offset: bits `[11:0]` (12 bits).  
Page number (VPN): bits `[63:12]` (52 bits).

**How about for a 64-bit system using 2MB pages? Which bits are used for page number and which are for page offset?**
Page offset: bits `[20:0]` (21 bits).  
Page number (VPN): bits `[63:21]` (43 bits).

**In a 2GB buffer, how many 2MB hugepages are there?**
`2GB / 2MB = 1024`, so there are **1024 hugepages**.

## 2-1

**Given a victim address 0x96ec3000, what is the value of its Row id? The value of its Column id?**
Using `ROW_MASK = 0xFFFE0000` and `COL_MASK = 0x1FFF`:
- Row id = `(0x96ec3000 & 0xFFFE0000) >> 17 = 0x4B76` (19318)
- Column id = `0x96ec3000 & 0x1FFF = 0x1000`

**For the same address, assume an arbitrary XOR function for computing the Bank id, list all possible attacker addresses whose Row id is one more than 0x96ec3000's Row id and all the other ids match, including the Bank id and Column id. Hint: there should be 16 such addresses total.**
Row `+1` adds `0x20000` (since row starts at bit 17), and keeping column fixed keeps the lower 13 bits as `0x1000`.  
The 16 possible attacker addresses (varying bits `[16:13]`) are:

`0x96ee1000, 0x96ee3000, 0x96ee5000, 0x96ee7000,`  
`0x96ee9000, 0x96eeb000, 0x96eed000, 0x96eef000,`  
`0x96ef1000, 0x96ef3000, 0x96ef5000, 0x96ef7000,`  
`0x96ef9000, 0x96efb000, 0x96efd000, 0x96eff000`.

## 2-3

**Analyze the statistics produced by your code when running part2, and report a threshold to distinguish the bank conflict.**

![Part2 Histogram](img/histo.png)

The histogram shows two separated groups:
- Low-latency group (likely non-conflict / different-bank): mostly `292–316` cycles (with a few outliers like `328`, `340`).
- High-latency group (likely bank conflict / same-bank different-row): `464–472` cycles.

A good threshold is **`390 cycles`** (any cutoff between ~`340` and `464` works; `390` gives a safe margin from both clusters).

## 3-2
**Based on the XOR function you reverse-engineered, determine which of the 16 candidate addresses you derived in Discussion Question 2-1 maps to the same bank.**

## 4-2

**Try different data pattern and include the bitflip observation statistics in the table below. Then answer the following questions:**

**Do your results match your expectations? What is the best pattern to trigger flips effectively?**

## 5-1

**Given the ECC type descriptions listed above, fill in the following table (assuming a data length of 4). For correction/detection, only answer "Yes" if it can always correct/detect (and "No" if there is ever a case where the scheme can fail to correct/detect). We've filled in the first line for you.**

## 5-3

**When a single bit flip is detected, describe how Hamming(22,16) can correct this error.**

## 5-5

**Can the Hamming(22,16) code we implemented always protect us from rowhammer attacks? If not, describe how a clever attacker could work around this scheme.**
