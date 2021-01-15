#pragma once

class iUhrType {
public:
    virtual void Letter_set(const uint16_t index) {
        uint8_t row = index / COLS_MATRIX();
        uint8_t col = index % COLS_MATRIX();
        if (row % 2 == 0) {
            col = COLS_MATRIX() - col - 1;
        }
        Word_array[row] = 1 << col;
    }

    virtual void show(uint8_t text) = 0;

    virtual inline const uint16_t NUM_PIXELS() = 0;

    virtual inline const uint16_t NUM_SMATRIX() = 0;

    virtual inline const uint16_t ROWS_MATRIX() = 0;

    virtual inline const uint16_t COLS_MATRIX() = 0;

    virtual inline const uint16_t NUM_RMATRIX() = 0;

    virtual const uint16_t getSMatrix(uint16_t index) = 0;

    virtual const uint16_t getRMatrix(uint16_t index) = 0;

    virtual const uint16_t getMatrix(uint8_t col, uint8_t row) {
        if (row % 2 == 0) {
            col = COLS_MATRIX() - col - 1;
        }
        return col * row;
    };

    virtual const uint16_t getMinArr(uint8_t col, uint8_t row) = 0;
};
