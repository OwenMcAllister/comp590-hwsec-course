#include "../ecc.hh"
#include "../verif.hh"

// Convenience Array
//
// Value for row x column y is 1 iff
// bit y is included in parity bit x
uint8_t parity_eqs[5][16] = {                 \
    {1,1,0,1,1,0,1,0,1,0,1,1,0,1,0,1},        \
    {1,0,1,1,0,1,1,0,0,1,1,0,1,1,0,0},        \
    {0,1,1,1,0,0,0,1,1,1,1,0,0,0,1,1},        \
    {0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0},        \
    {0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1}
};

// Generates the parity bits (P5-P0) for a given input ("data")
uint32_t genParity(uint32_t data) {
    uint32_t parity = 0;

    // data is the data of the decoded hamming_struct
    for (int p = 0; p < NUM_PARITY_BITS - 1; p++) {
        uint8_t parity_bit = 0;
        for (int bit = 0; bit < NUM_DATA_BITS; bit++) {
            if (parity_eqs[p][bit]) { // It is included in calculating parity bit p
                // printf("Data bit %d value: %d contributes to parity bit %d\n", bit, getBit(data, bit), p);
                parity_bit ^= getBit(data, bit); // XOR the corresponding data bits together to get the parity bit value
            }
        }
        parity |= parity_bit << p; // Set parity bit p in the parity output
        // printf("Parity bit %d value: %d\n", p, parity_bit);
    }

    // P5 is the XOR of all data bits and parity bits (P0-P4)
    // Overall parity bit is computed by XORing all 22 bits together

    uint8_t p5 = 0;
    for (int bit = 0; bit < NUM_DATA_BITS; bit++) {
        p5 ^= getBit(data, bit);
    } // 1. XOR all the data bits together to get an intermediate parity value for P5
    
    for (int p = 0; p < NUM_PARITY_BITS - 1; p++) {
        p5 ^= getBit(parity, p);
    } // 2. XOR the other parity bits together to get the overall parity bit value (P5)

    // 3. Add the overall parity bit (P5) to the parity output
    parity |= ((uint32_t)p5) << (NUM_PARITY_BITS - 1);

    printf("Overall parity bit (P5) value: %d\n", p5);
    printf("Final parity value (P5-P0): %x\n", parity);

    return parity;
}

// Determine if there are any errors, reporting the error type and syndrome.
struct hamming_result findHammingErrors(uint32_t encoded) {

    // Break down the ECC protected value into parity and data 
    hamming_struct decoded = extractEncoding(encoded);

    // Record the parity in the data, and also re-generate the parity
    // from the data
    uint32_t recordedParity = decoded.parity;
    uint32_t regenParity = genParity(decoded.data);

    // TODO: Exercise 5-4, Compute the syndrome
    uint32_t syndrome = 0;
    // XOR extracted parity with regenerated parity to get the syndrome
    // Only use P0-P4 for syndrome calculation, exclude P5
    syndrome = (recordedParity ^ regenParity) & ((1U << (NUM_PARITY_BITS - 1)) - 1);

    // TODO: Exercise 5-4, Compute P5 Error bit
    uint32_t P5_Error_bit = 0;
    // Overall parity bit is the XOR of all bits in the encoded value, which is P5
    for (int i = 0; i < TOTAL_BITS; i++) {
        P5_Error_bit ^= getBit(encoded, i);
    }
 
    // TODO: Exercise 5-4, Determine the error type
    _ERROR_TYPE error = NO_ERROR;
    if (syndrome == 0 && P5_Error_bit == 0) {
        error = NO_ERROR; // No error if syndrome is 0 and overall parity bit is correct
    } else if (syndrome != 0 && P5_Error_bit == 1) {
        error = SINGLE_ERROR; // Single bit error if syndrome is not 0 and overall parity bit is incorrect
    } else if (syndrome != 0 && P5_Error_bit == 0) {
        error = DOUBLE_ERROR; // Double bit error if syndrome is not 0 but overall parity bit is correct
    } else if (syndrome == 0 && P5_Error_bit == 1) {
        error = PARITY_ERROR; // Parity bit error if syndrome is 0 but overall parity bit is incorrect
    }

    printf("--- Debug Info ---\n");
    printf("Recorded Parity: %x, Regenerated Parity: %x, Syndrome: %x, P5_Error_bit: %d\n", recordedParity, regenParity, syndrome, P5_Error_bit);
    printf("Error Type: %d\n", error);
    
    return {error, syndrome};
}

// Use findHammingErrors to check if there's an error
// If there's an error, correct it!
uint32_t verifyAndRepair(uint32_t encoded) {

    // Determine the error type
    struct hamming_result result = findHammingErrors(encoded);

    // TODO: Exercise 5-4, If the error type is correctable, correct it here!
    uint32_t out = encoded;
    if (result.error == SINGLE_ERROR) {
        // If there's a single error, we can correct it by flipping the bit indicated by the syndrome
        // Hamming syndrome is 1-based encoded bit position, so subtract 1 to get the 0-based bit index
        uint32_t errorBit = result.syndrome - 1;
        printf("Single error detected! Syndrome indicates bit index %d is flipped.\n", errorBit);
        out = flipBit(encoded, errorBit);
    } else if (result.error == PARITY_ERROR) {
        printf("Parity error detected! Flipping the overall parity bit to correct it.\n");
        // If there's a parity error, we can flip the overall parity bit (the last bit) to correct it
        out = flipBit(encoded, TOTAL_BITS - 1);
    }

    return out;
}
/*
 *
 * DO NOT MODIFY BELOW ME
 *
 */

int main(void) {

    /* -------- Setup Phase -------- */

    // Generate Random Data
    int numTests = 5;
    int pass = 1;
    for (int testIter = 0; testIter < numTests; testIter++) {
        printf("=== Test Iteration %d / %d ===\n", testIter+1, numTests);

        srand (time(NULL));
        uint32_t data = rand() % (1 << NUM_DATA_BITS);
        printf("Random Data Generated: %x\n", data);

        // Generate Data Parity
        uint32_t parity = genParity(data);
        printf("Parity of Generated Data: %x\n", parity);

        // Encode into bitstream
        uint32_t encoded = embedEncoding({data, parity});
        printf("Embedded Encoding: %x\n", encoded);

        // Check result against gold solution
        if(!checkParity(encoded)) {
            printf("FAIL: Not quite! Your parity value is incorrect.\n");
            return -1;
        } else {
            printf("PASS: Your parity value is correct.\n");
        }

        /* -------- Verify No Error Case --------- */


        // Verify that no errors occur upon 0 bit flips
        printf("\nVerifying (no flips)...\n");
        if (findHammingErrors(encoded).error != NO_ERROR) {
            printf("FAIL: Error detected when there wasn't one!\n");
            pass = 0;
        } else if (encoded != verifyAndRepair(encoded)) {
            printf("FAIL: verifyAndRepair changed a valid encoding!\n");
            pass = 0;
        } else {
            printf("PASS.\n");
        }
        
        /* -------- Verify One Error Case --------- */

        printf("\nVerifying (one flip)...\n");

        // Inject errors into encoding    
        uint32_t encoded_oneflip = injectRandomFlips(encoded,1);
        printf("Data after one bit flip: %x\n", encoded_oneflip);

        // Verify 1 bit flip case    
        uint32_t repaired = verifyAndRepair(encoded_oneflip);
        printf("Repaired Encoding: %x\n", repaired);

        if (findHammingErrors(encoded_oneflip).error != SINGLE_ERROR) {
            printf("FAIL: Single error not detected!\n");
            pass = 0;
        } else if (encoded != verifyAndRepair(encoded_oneflip)) {
            printf("FAIL: verifyAndRepair failed to correct value!\n");
            pass = 0;
        } else {
            printf("PASS.\n");
        }
                
        /* -------- Verify Parity Error Case --------- */
        
        printf("\nVerifying (parity flip)...\n");
        
        // Inject errors into encoding    
        uint32_t encoded_parityflip = flipBit(encoded,TOTAL_BITS-1);
        printf("Data after parity bit flip: %x\n", encoded_parityflip);

        // Verify 1 bit flip case    
        repaired = verifyAndRepair(encoded_parityflip);
        printf("Repaired Encoding: %x\n", repaired);
        
        if (findHammingErrors(encoded_parityflip).error != PARITY_ERROR) {
            printf("FAIL: Parity error not detected!\n");
            pass = 0;
        } else if (encoded != verifyAndRepair(encoded_parityflip)) {
            printf("FAIL: verifyAndRepair failed to correct value!\n");
            pass = 0;
        } else {
            printf("PASS.\n");
        }
        
        /* -------- Verify Double Error Case --------- */
        
        printf("\nVerifying (double flip)...\n");
        
        // Inject 2 errors into encoding
        uint32_t encoded_twoflip = injectRandomFlips(encoded,2);
        printf("Two bit flip: %x\n", encoded_twoflip);

        if (findHammingErrors(encoded_twoflip).error != DOUBLE_ERROR) {
            printf("FAIL: Parity error not detected!\n");
            pass = 0;
        } else {
            printf("PASS.\n");
        }

        if (!pass) {
            printf("\n*** One or more tests failed. ***\n");
            break;
        }
    } 
    /* -------- Finish --------- */

    if(pass) {
        printf("\n*** All tests passed, congratulations! ***\n");
    }

}
