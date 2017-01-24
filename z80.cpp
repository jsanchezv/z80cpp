// Converted to C++ from Java at
//... https://github.com/jsanchezv/Z80Core
//... commit c4f267e3564fa89bd88fd2d1d322f4d6b0069dbd
//... GPL 3
//... v0.0.6 (24/01/2017)
//    quick & dirty conversion by dddddd (AKA deesix)

//... compile with $ g++ -m32 -std=c++14
//... put the zen*bin files in the same directory.

#include <iostream>
#include <fstream>
#include <cstdint>
using namespace std;

class Klock {
public:

    uint32_t getTstates() {
        return tstates;
    }

    void setTstates(uint32_t nstates) {
        tstates = nstates;
    }

    void addTstates(uint32_t nstates) {
        tstates += nstates;
    }

    void reset() {
        cout << "Klock.reset() called!" << endl;
        frames = timeout = tstates = 0;
    }

    void setTimeout(uint32_t ntstates) {
    }

private:
    uint32_t tstates;
    uint32_t frames;
    uint32_t timeout;
};

class Z80; // forward declaration.

class Z80operations {
private:
    Klock klock;
    uint8_t z80Ram[0x10000];
    uint8_t z80Ports[0x10000];
    bool finish = true;

public:
    Z80* z80;
    Z80operations();
    uint8_t fetchOpcode(uint16_t address);

    uint8_t peek8(uint16_t address);
    void poke8(uint16_t address, uint8_t value);
    uint16_t peek16(uint16_t address);
    void poke16(uint16_t address, uint16_t word);

    uint8_t inPort(uint16_t port);
    void outPort(uint16_t port, uint8_t value);

    void contendedStates(uint16_t address, uint32_t tstates);

    void breakpoint();
    void execDone();

    void runTest(ifstream* file);
};

class Z80 {
public:
    // Modos de interrupción

    enum IntMode {
        IM0, IM1, IM2
    };

private:
    Klock klock;
    // Código de instrucción a ejecutar
    uint8_t opCode;
    // Subsistema de notificaciones
    bool execDone;
    // Posiciones de los flags
    const static unsigned int CARRY_MASK = 0x01;
    const static unsigned int ADDSUB_MASK = 0x02;
    const static unsigned int PARITY_MASK = 0x04;
    const static unsigned int OVERFLOW_MASK = 0x04; // alias de PARITY_MASK
    const static unsigned int BIT3_MASK = 0x08;
    const static unsigned int HALFCARRY_MASK = 0x10;
    const static unsigned int BIT5_MASK = 0x20;
    const static unsigned int ZERO_MASK = 0x40;
    const static unsigned int SIGN_MASK = 0x80;
    // Máscaras de conveniencia
    const static unsigned int FLAG_53_MASK = BIT5_MASK | BIT3_MASK;
    const static unsigned int FLAG_SZ_MASK = SIGN_MASK | ZERO_MASK;
    const static unsigned int FLAG_SZHN_MASK = FLAG_SZ_MASK | HALFCARRY_MASK | ADDSUB_MASK;
    const static unsigned int FLAG_SZP_MASK = FLAG_SZ_MASK | PARITY_MASK;
    const static unsigned int FLAG_SZHP_MASK = FLAG_SZP_MASK | HALFCARRY_MASK;
    // Acumulador y resto de registros de 8 bits
    unsigned int regA, regB, regC, regD, regE, regH, regL;
    // Flags sIGN, zERO, 5, hALFCARRY, 3, pARITY y ADDSUB (n)
    unsigned int sz5h3pnFlags;
    // El flag Carry es el único que se trata aparte
    bool carryFlag;
    /* Flags para indicar la modificación del registro F en la instrucción actual
     * y en la anterior.
     * Son necesarios para emular el comportamiento de los bits 3 y 5 del
     * registro F con las instrucciones CCF/SCF.
     * 
     * http://www.worldofspectrum.org/forums/showthread.php?t=41834
     * http://www.worldofspectrum.org/forums/showthread.php?t=41704
     * 
     * Thanks to Patrik Rak for his tests and investigations.
     */
    bool flagQ, lastFlagQ;
    // Acumulador alternativo y flags -- 8 bits
    unsigned int regAx;
    unsigned int regFx;
    // Registros alternativos
    unsigned int regBx, regCx, regDx, regEx, regHx, regLx;
    // Registros de propósito específico
    // *PC -- Program Counter -- 16 bits*
    uint16_t regPC;
    // *IX -- Registro de índice -- 16 bits*
    unsigned int regIX;
    // *IY -- Registro de índice -- 16 bits*
    unsigned int regIY;
    // *SP -- Stack Pointer -- 16 bits*
    unsigned int regSP;
    // *I -- Vector de interrupción -- 8 bits*
    unsigned int regI;
    // *R -- Refresco de memoria -- 7 bits*
    unsigned int regR;
    // *R7 -- Refresco de memoria -- 1 bit* (bit superior de R)
    bool regRbit7;
    //Flip-flops de interrupción
    bool ffIFF1 = false;
    bool ffIFF2 = false;
    // EI solo habilita las interrupciones DESPUES de ejecutar la
    // siguiente instrucción (excepto si la siguiente instrucción es un EI...)
    bool pendingEI = false;
    // Estado de la línea NMI
    bool activeNMI = false;
    // Si está activa la línea INT
    // En el 48 y los +2a/+3 la línea INT se activa durante 32 ciclos de reloj
    // En el 128 y +2, se activa 36 ciclos de reloj
    bool activeINT = false;
    // Modo de interrupción
    IntMode modeINT = IntMode::IM0;
    // halted == true cuando la CPU está ejecutando un HALT (28/03/2010)
    bool halted = false;
    // pinReset == true, se ha producido un reset a través de la patilla
    bool pinReset = false;
    /*
     * Registro interno que usa la CPU de la siguiente forma
     *
     * ADD HL,xx      = Valor del registro H antes de la suma
     * LD r,(IX/IY+d) = Byte superior de la suma de IX/IY+d
     * JR d           = Byte superior de la dirección de destino del salto
     *
     * 04/12/2008     No se vayan todavía, aún hay más. Con lo que se ha
     *                implementado hasta ahora parece que funciona. El resto de
     *                la historia está contada en:
     *                http://zx.pk.ru/attachment.php?attachmentid=2989
     *
     * 25/09/2009     Se ha completado la emulación de MEMPTR. A señalar que
     *                no se puede comprobar si MEMPTR se ha emulado bien hasta
     *                que no se emula el comportamiento del registro en las
     *                instrucciones CPI y CPD. Sin ello, todos los tests de
     *                z80tests.tap fallarán aunque se haya emulado bien al
     *                registro en TODAS las otras instrucciones.
     *                Shit yourself, little parrot.
     */
    uint16_t memptr;

    /* Algunos flags se precalculan para un tratamiento más rápido
     * Concretamente, SIGN, ZERO, los bits 3, 5, PARITY y ADDSUB:
     * sz53n_addTable tiene el ADDSUB flag a 0 y paridad sin calcular
     * sz53pn_addTable tiene el ADDSUB flag a 0 y paridad calculada
     * sz53n_subTable tiene el ADDSUB flag a 1 y paridad sin calcular
     * sz53pn_subTable tiene el ADDSUB flag a 1 y paridad calculada
     * El resto de bits están a 0 en las cuatro tablas lo que es
     * importante para muchas operaciones que ponen ciertos flags a 0 por real
     * decreto. Si lo ponen a 1 por el mismo método basta con hacer un OR con
     * la máscara correspondiente.
     */
    unsigned int sz53n_addTable[256];
    unsigned int sz53pn_addTable[256];
    unsigned int sz53n_subTable[256];
    unsigned int sz53pn_subTable[256];

    // d6 Bloque static{} movido a reset().

    // Un true en una dirección indica que se debe notificar que se va a
    // ejecutar la instrucción que está en esa direción.
    bool breakpointAt[65536];

    void incRegBC() {
        if (++regC < 0x100) {
            return;
        }

        regC = 0;

        if (++regB < 0x100) {
            return;
        }

        regB = 0;
    }

    void decRegBC() {
        if (--regC >= 0) {
            return;
        }

        regC = 0xff;

        if (--regB >= 0) {
            return;
        }

        regB = 0xff;
    }

    void incRegDE() {
        if (++regE < 0x100) {
            return;
        }

        regE = 0;

        if (++regD < 0x100) {
            return;
        }

        regD = 0;
    }

    void decRegDE() {
        if (--regE >= 0) {
            return;
        }

        regE = 0xff;

        if (--regD >= 0) {
            return;
        }

        regD = 0xff;
    }

    void incRegHL() {
        if (++regL < 0x100) {
            return;
        }

        regL = 0;

        if (++regH < 0x100) {
            return;
        }

        regH = 0;
    }

    void decRegHL() {
        if (--regL >= 0) {
            return;
        }

        regL = 0xff;

        if (--regH >= 0) {
            return;
        }

        regH = 0xff;
    }

public:
    Z80operations Z80opsImpl;
    // Constructor de la clase

    Z80() {
        Z80opsImpl.z80 = this;
        execDone = false;
        resetBreakpoints();
        reset();
        cout << "Terminando constructor de Z80" << endl;
    }

    // Acceso a registros de 8 bits

    unsigned int getRegA() {
        return regA;
    }

    void setRegA(unsigned int value) {
        regA = value & 0xff;
    }

    unsigned int getRegB() {
        return regB;
    }

    void setRegB(unsigned int value) {
        regB = value & 0xff;
    }

    unsigned int getRegC() {
        return regC;
    }

    void setRegC(unsigned int value) {
        regC = value & 0xff;
    }

    unsigned int getRegD() {
        return regD;
    }

    void setRegD(unsigned int value) {
        regD = value & 0xff;
    }

    unsigned int getRegE() {
        return regE;
    }

    void setRegE(unsigned int value) {
        regE = value & 0xff;
    }

    unsigned int getRegH() {
        return regH;
    }

    void setRegH(unsigned int value) {
        regH = value & 0xff;
    }

    unsigned int getRegL() {
        return regL;
    }

    void setRegL(unsigned int value) {
        regL = value & 0xff;
    }

    // Acceso a registros alternativos de 8 bits

    unsigned int getRegAx() {
        return regAx;
    }

    void setRegAx(unsigned int value) {
        regAx = value & 0xff;
    }

    unsigned int getRegFx() {
        return regFx;
    }

    void setRegFx(unsigned int value) {
        regFx = value & 0xff;
    }

    unsigned int getRegBx() {
        return regBx;
    }

    void setRegBx(unsigned int value) {
        regBx = value & 0xff;
    }

    unsigned int getRegCx() {
        return regCx;
    }

    void setRegCx(unsigned int value) {
        regCx = value & 0xff;
    }

    unsigned int getRegDx() {
        return regDx;
    }

    void setRegDx(unsigned int value) {
        regDx = value & 0xff;
    }

    unsigned int getRegEx() {
        return regEx;
    }

    void setRegEx(unsigned int value) {
        regEx = value & 0xff;
    }

    unsigned int getRegHx() {
        return regHx;
    }

    void setRegHx(unsigned int value) {
        regHx = value & 0xff;
    }

    unsigned int getRegLx() {
        return regLx;
    }

    void setRegLx(unsigned int value) {
        regLx = value & 0xff;
    }

    // Acceso a registros de 16 bits

    unsigned int getRegAF() {
        return (regA << 8) | (carryFlag ? sz5h3pnFlags | CARRY_MASK : sz5h3pnFlags);
    }

    void setRegAF(unsigned int word) {
        regA = (word >> 8) & 0xff;

        sz5h3pnFlags = word & 0xfe;
        carryFlag = (word & CARRY_MASK) != 0;
    }

    unsigned int getRegAFx() {
        return (regAx << 8) | regFx;
    }

    void setRegAFx(unsigned int word) {
        regAx = (word >> 8) & 0xff;
        regFx = word & 0xff;
    }

    unsigned int getRegBC() {
        return (regB << 8) | regC;
    }

    void setRegBC(unsigned int word) {
        regB = (word >> 8) & 0xff;
        regC = word & 0xff;
    }

    unsigned int getRegBCx() {
        return (regBx << 8) | regCx;
    }

    void setRegBCx(unsigned int word) {
        regBx = (word >> 8) & 0xff;
        regCx = word & 0xff;
    }

    unsigned int getRegDE() {
        return (regD << 8) | regE;
    }

    void setRegDE(unsigned int word) {
        regD = (word >> 8) & 0xff;
        regE = word & 0xff;
    }

    unsigned int getRegDEx() {
        return (regDx << 8) | regEx;
    }

    void setRegDEx(unsigned int word) {
        regDx = (word >> 8) & 0xff;
        regEx = word & 0xff;
    }

    unsigned int getRegHL() {
        return (regH << 8) | regL;
    }

    void setRegHL(unsigned int word) {
        regH = (word >> 8) & 0xff;
        regL = word & 0xff;
    }

    /* Las funciones incRegXX y decRegXX están escritas pensando en que
     * puedan aprovechar el camino más corto aunque tengan un poco más de
     * código (al menos en bytecodes lo tienen)
     */
    unsigned int getRegHLx() {
        return (regHx << 8) | regLx;
    }

    void setRegHLx(unsigned int word) {
        regHx = (word >> 8) & 0xff;
        regLx = word & 0xff;
    }

    // Acceso a registros de propósito específico

    uint16_t getRegPC() {
        return regPC;
    }

    void setRegPC(uint16_t address) {
        regPC = address;
    }

    uint16_t getRegSP() {
        return regSP;
    }

    void setRegSP(uint16_t word) {
        regSP = word;
    }

    unsigned int getRegIX() {
        return regIX;
    }

    void setRegIX(unsigned int word) {
        regIX = word & 0xffff;
    }

    unsigned int getRegIY() {
        return regIY;
    }

    void setRegIY(unsigned int word) {
        regIY = word & 0xffff;
    }

    unsigned int getRegI() {
        return regI;
    }

    void setRegI(unsigned int value) {
        regI = value & 0xff;
    }

    unsigned int getRegR() {
        return regRbit7 ? (regR & 0x7f) | SIGN_MASK : regR & 0x7f;
    }

    void setRegR(unsigned int value) {
        regR = value & 0x7f;
        regRbit7 = (value > 0x7f);
    }

    unsigned int getPairIR() {
        if (regRbit7) {
            return (regI << 8) | ((regR & 0x7f) | SIGN_MASK);
        }
        return (regI << 8) | (regR & 0x7f);
    }

    // Acceso al registro oculto MEMPTR

    uint16_t getMemPtr() {
        return memptr;
    }

    void setMemPtr(uint16_t word) {
        memptr = word;
    }

    // Acceso a los flags uno a uno

    bool isCarryFlag() {
        return carryFlag;
    }

    void setCarryFlag(bool state) {
        carryFlag = state;
    }

    bool isAddSubFlag() {
        return (sz5h3pnFlags & ADDSUB_MASK) != 0;
    }

    void setAddSubFlag(bool state) {
        if (state) {
            sz5h3pnFlags |= ADDSUB_MASK;
        } else {
            sz5h3pnFlags &= ~ADDSUB_MASK;
        }
    }

    bool isParOverFlag() {
        return (sz5h3pnFlags & PARITY_MASK) != 0;
    }

    void setParOverFlag(bool state) {
        if (state) {
            sz5h3pnFlags |= PARITY_MASK;
        } else {
            sz5h3pnFlags &= ~PARITY_MASK;
        }
    }

    bool isBit3Flag() {
        return (sz5h3pnFlags & BIT3_MASK) != 0;
    }

    void setBit3Fag(bool state) {
        if (state) {
            sz5h3pnFlags |= BIT3_MASK;
        } else {
            sz5h3pnFlags &= ~BIT3_MASK;
        }
    }

    bool isHalfCarryFlag() {
        return (sz5h3pnFlags & HALFCARRY_MASK) != 0;
    }

    void setHalfCarryFlag(bool state) {
        if (state) {
            sz5h3pnFlags |= HALFCARRY_MASK;
        } else {
            sz5h3pnFlags &= ~HALFCARRY_MASK;
        }
    }

    bool isBit5Flag() {
        return (sz5h3pnFlags & BIT5_MASK) != 0;
    }

    void setBit5Flag(bool state) {
        if (state) {
            sz5h3pnFlags |= BIT5_MASK;
        } else {
            sz5h3pnFlags &= ~BIT5_MASK;
        }
    }

    bool isZeroFlag() {
        return (sz5h3pnFlags & ZERO_MASK) != 0;
    }

    void setZeroFlag(bool state) {
        if (state) {
            sz5h3pnFlags |= ZERO_MASK;
        } else {
            sz5h3pnFlags &= ~ZERO_MASK;
        }
    }

    bool isSignFlag() {
        return (sz5h3pnFlags & SIGN_MASK) != 0;
    }

    void setSignFlag(bool state) {
        if (state) {
            sz5h3pnFlags |= SIGN_MASK;
        } else {
            sz5h3pnFlags &= ~SIGN_MASK;
        }
    }

    // Acceso a los flags F

    unsigned int getFlags() {
        return carryFlag ? sz5h3pnFlags | CARRY_MASK : sz5h3pnFlags;
    }

    void setFlags(unsigned int regF) {
        sz5h3pnFlags = regF & 0xfe;

        carryFlag = (regF & CARRY_MASK) != 0;
    }

    // Acceso a los flip-flops de interrupción

    bool isIFF1() {
        return ffIFF1;
    }

    void setIFF1(bool state) {
        ffIFF1 = state;
    }

    bool isIFF2() {
        return ffIFF2;
    }

    void setIFF2(bool state) {
        ffIFF2 = state;
    }

    bool isNMI() {
        return activeNMI;
    }

    void setNMI(bool nmi) {
        activeNMI = nmi;
    }

    // La línea de NMI se activa por impulso, no por nivel

    void triggerNMI() {
        activeNMI = true;
    }

    // La línea INT se activa por nivel

    bool isINTLine() {
        return activeINT;
    }

    void setINTLine(bool intLine) {
        activeINT = intLine;
    }

    //Acceso al modo de interrupción

    IntMode getIM() {
        return modeINT;
    }

    void setIM(IntMode mode) {
        modeINT = mode;
    }

    bool isHalted() {
        return halted;
    }

    void setHalted(bool state) {
        halted = state;
    }

    void setPinReset() {
        pinReset = true;
    }

    bool isPendingEI() {
        return pendingEI;
    }

    void setPendingEI(bool state) {
        pendingEI = state;
    }

    //     Z80State getZ80State() {
    //         Z80State state = new Z80State();
    //         state.setRegA(regA);
    //         state.setRegF(getFlags());
    //         state.setRegB(regB);
    //         state.setRegC(regC);
    //         state.setRegD(regD);
    //         state.setRegE(regE);
    //         state.setRegH(regH);
    //         state.setRegL(regL);
    //         state.setRegAx(regAx);
    //         state.setRegFx(regFx);
    //         state.setRegBx(regBx);
    //         state.setRegCx(regCx);
    //         state.setRegDx(regDx);
    //         state.setRegEx(regEx);
    //         state.setRegHx(regHx);
    //         state.setRegLx(regLx);
    //         state.setRegIX(regIX);
    //         state.setRegIY(regIY);
    //         state.setRegSP(regSP);
    //         state.setRegPC(regPC);
    //         state.setRegI(regI);
    //         state.setRegR(getRegR());
    //         state.setMemPtr(memptr);
    //         state.setHalted(halted);
    //         state.setIFF1(ffIFF1);
    //         state.setIFF2(ffIFF2);
    //         state.setIM(modeINT);
    //         state.setINTLine(activeINT);
    //         state.setPendingEI(pendingEI);
    //         state.setNMI(activeNMI);
    //         state.setFlagQ(lastFlagQ);
    //         return state;
    //     }
    //     
    //     void setZ80State(Z80State state) {
    //         regA = state.getRegA();
    //         setFlags(state.getRegF());
    //         regB = state.getRegB();
    //         regC = state.getRegC();
    //         regD = state.getRegD();
    //         regE = state.getRegE();
    //         regH = state.getRegH();
    //         regL = state.getRegL();
    //         regAx = state.getRegAx();
    //         regFx = state.getRegFx();
    //         regBx = state.getRegBx();
    //         regCx = state.getRegCx();
    //         regDx = state.getRegDx();
    //         regEx = state.getRegEx();
    //         regHx = state.getRegHx();
    //         regLx = state.getRegLx();
    //         regIX = state.getRegIX();
    //         regIY = state.getRegIY();
    //         regSP = state.getRegSP();
    //         regPC = state.getRegPC();
    //         regI = state.getRegI();
    //         setRegR(state.getRegR());
    //         memptr = state.getMemPtr();
    //         halted = state.isHalted();
    //         ffIFF1 = state.isIFF1();
    //         ffIFF2 = state.isIFF2();
    //         modeINT = state.getIM();
    //         activeINT = state.isINTLine();
    //         pendingEI = state.isPendingEI();
    //         activeNMI = state.isNMI();
    //         flagQ = false;
    //         lastFlagQ = state.isFlagQ();
    //     }

    // Reset

    /* Según el documento de Sean Young, que se encuentra en
     * [http://www.myquest.com/z80undocumented], la mejor manera de emular el
     * reset es poniendo PC, IFF1, IFF2, R e IM0 a 0 y todos los demás registros
     * a 0xFFFF.
     * 
     * 29/05/2011: cuando la CPU recibe alimentación por primera vez, los
     *             registros PC e IR se inicializan a cero y el resto a 0xFF.
     *             Si se produce un reset a través de la patilla correspondiente,
     *             los registros PC e IR se inicializan a 0 y el resto se preservan.
     *             En cualquier caso, todo parece depender bastante del modelo
     *             concreto de Z80, así que se escoge el comportamiento del
     *             modelo Zilog Z8400APS. Z80A CPU.
     *             http://www.worldofspectrum.org/forums/showthread.php?t=34574
     */
    void reset() {
        if (pinReset) {
            pinReset = false;
        } else {
            regA = regAx = 0xff;
            setFlags(0xff);
            regFx = 0xff;
            regB = regBx = 0xff;
            regC = regCx = 0xff;
            regD = regDx = 0xff;
            regE = regEx = 0xff;
            regH = regHx = 0xff;
            regL = regLx = 0xff;

            regIX = regIY = 0xffff;

            regSP = 0xffff;

            memptr = 0xffff;
        }

        regPC = 0;
        regI = regR = 0;
        regRbit7 = false;
        ffIFF1 = false;
        ffIFF2 = false;
        pendingEI = false;
        activeNMI = false;
        activeINT = false;
        halted = false;
        setIM(IntMode::IM0);
        lastFlagQ = false;

        // d6
        bool evenBits;

        for (unsigned int idx = 0; idx < 256; idx++) {
            if (idx > 0x7f) {
                sz53n_addTable[idx] |= SIGN_MASK;
            }

            evenBits = true;
            for (unsigned int mask = 0x01; mask < 0x100; mask <<= 1) {
                if ((idx & mask) != 0) {
                    evenBits = !evenBits;
                }
            }

            sz53n_addTable[idx] |= (idx & FLAG_53_MASK);
            sz53n_subTable[idx] = sz53n_addTable[idx] | ADDSUB_MASK;

            if (evenBits) {
                sz53pn_addTable[idx] = sz53n_addTable[idx] | PARITY_MASK;
                sz53pn_subTable[idx] = sz53n_subTable[idx] | PARITY_MASK;
            } else {
                sz53pn_addTable[idx] = sz53n_addTable[idx];
                sz53pn_subTable[idx] = sz53n_subTable[idx];
            }
        }

        sz53n_addTable[0] |= ZERO_MASK;
        sz53pn_addTable[0] |= ZERO_MASK;
        sz53n_subTable[0] |= ZERO_MASK;
        sz53pn_subTable[0] |= ZERO_MASK;
    }

    // Rota a la izquierda el valor del argumento
    // El bit 0 y el flag C toman el valor del bit 7 antes de la operación

    unsigned int rlc(unsigned int oper8) {
        carryFlag = (oper8 > 0x7f);
        oper8 = (oper8 << 1) & 0xfe;
        if (carryFlag) {
            oper8 |= CARRY_MASK;
        }
        sz5h3pnFlags = sz53pn_addTable[oper8];
        flagQ = true;
        return oper8;
    }

    // Rota a la izquierda el valor del argumento
    // El bit 7 va al carry flag
    // El bit 0 toma el valor del flag C antes de la operación

    unsigned int rl(unsigned int oper8) {
        bool carry = carryFlag;
        carryFlag = (oper8 > 0x7f);
        oper8 = (oper8 << 1) & 0xfe;
        if (carry) {
            oper8 |= CARRY_MASK;
        }
        sz5h3pnFlags = sz53pn_addTable[oper8];
        flagQ = true;
        return oper8;
    }

    // Rota a la izquierda el valor del argumento
    // El bit 7 va al carry flag
    // El bit 0 toma el valor 0

    unsigned int sla(unsigned int oper8) {
        carryFlag = (oper8 > 0x7f);
        oper8 = (oper8 << 1) & 0xfe;
        sz5h3pnFlags = sz53pn_addTable[oper8];
        flagQ = true;
        return oper8;
    }

    // Rota a la izquierda el valor del argumento (como sla salvo por el bit 0)
    // El bit 7 va al carry flag
    // El bit 0 toma el valor 1
    // Instrucción indocumentada

    unsigned int sll(unsigned int oper8) {
        carryFlag = (oper8 > 0x7f);
        oper8 = ((oper8 << 1) | CARRY_MASK) & 0xff;
        sz5h3pnFlags = sz53pn_addTable[oper8];
        flagQ = true;
        return oper8;
    }

    // Rota a la derecha el valor del argumento
    // El bit 7 y el flag C toman el valor del bit 0 antes de la operación

    unsigned int rrc(unsigned int oper8) {
        carryFlag = (oper8 & CARRY_MASK) != 0;
        oper8 >>= 1;
        if (carryFlag) {
            oper8 |= SIGN_MASK;
        }
        sz5h3pnFlags = sz53pn_addTable[oper8];
        flagQ = true;
        return oper8;
    }

    // Rota a la derecha el valor del argumento
    // El bit 0 va al carry flag
    // El bit 7 toma el valor del flag C antes de la operación

    unsigned int rr(unsigned int oper8) {
        bool carry = carryFlag;
        carryFlag = (oper8 & CARRY_MASK) != 0;
        oper8 >>= 1;
        if (carry) {
            oper8 |= SIGN_MASK;
        }
        sz5h3pnFlags = sz53pn_addTable[oper8];
        flagQ = true;
        return oper8;
    }

    // A = A7 A6 A5 A4 (HL)3 (HL)2 (HL)1 (HL)0
    // (HL) = A3 A2 A1 A0 (HL)7 (HL)6 (HL)5 (HL)4
    // Los bits 3,2,1 y 0 de (HL) se copian a los bits 3,2,1 y 0 de A.
    // Los 4 bits bajos que había en A se copian a los bits 7,6,5 y 4 de (HL).
    // Los 4 bits altos que había en (HL) se copian a los 4 bits bajos de (HL)
    // Los 4 bits superiores de A no se tocan. ¡p'habernos matao!

    void rrd() {
        unsigned int aux = (regA & 0x0f) << 4;
        memptr = getRegHL();
        uint16_t memHL = Z80opsImpl.peek8(memptr);
        regA = (regA & 0xf0) | (memHL & 0x0f);
        Z80opsImpl.contendedStates(memptr, 4);
        Z80opsImpl.poke8(memptr, (memHL >> 4) | aux);
        sz5h3pnFlags = sz53pn_addTable[regA];
        memptr++;
        flagQ = true;
    }

    // A = A7 A6 A5 A4 (HL)7 (HL)6 (HL)5 (HL)4
    // (HL) = (HL)3 (HL)2 (HL)1 (HL)0 A3 A2 A1 A0
    // Los 4 bits bajos que había en (HL) se copian a los bits altos de (HL).
    // Los 4 bits altos que había en (HL) se copian a los 4 bits bajos de A
    // Los bits 3,2,1 y 0 de A se copian a los bits 3,2,1 y 0 de (HL).
    // Los 4 bits superiores de A no se tocan. ¡p'habernos matao!

    void rld() {
        unsigned int aux = regA & 0x0f;
        memptr = getRegHL();
        uint16_t memHL = Z80opsImpl.peek8(memptr);
        regA = (regA & 0xf0) | (memHL >> 4);
        Z80opsImpl.contendedStates(memptr, 4);
        Z80opsImpl.poke8(memptr, ((memHL << 4) | aux) & 0xff);
        sz5h3pnFlags = sz53pn_addTable[regA];
        memptr++;
        flagQ = true;
    }

    // Rota a la derecha 1 bit el valor del argumento
    // El bit 0 pasa al carry.
    // El bit 7 conserva el valor que tenga

    unsigned int sra(unsigned int oper8) {
        unsigned int sign = oper8 & SIGN_MASK;
        carryFlag = (oper8 & CARRY_MASK) != 0;
        // d6 Java >> not converted/analized
        oper8 = (oper8 >> 1) | sign;
        sz5h3pnFlags = sz53pn_addTable[oper8];
        flagQ = true;
        return oper8;
    }

    // Rota a la derecha 1 bit el valor del argumento
    // El bit 0 pasa al carry.
    // El bit 7 toma el valor 0

    unsigned int srl(unsigned int oper8) {
        carryFlag = (oper8 & CARRY_MASK) != 0;
        oper8 >>= 1;
        sz5h3pnFlags = sz53pn_addTable[oper8];
        flagQ = true;
        return oper8;
    }

    /*
     * Half-carry flag:
     * 
     * FLAG = (A^B^RESULT)&0x10  for any operation
     * 
     * Overflow flag:
     * 
     * FLAG = ~(A^B)&(B^RESULT)&0x80 for addition [ADD/ADC]
     * FLAG = (A^B)&(A^RESULT)&0x80  for subtraction [SUB/SBC]
     * 
     * For INC/DEC, you can use following simplifications:
     * 
     * INC:
     * H_FLAG = (RESULT&0x0F)==0x00
     * V_FLAG = RESULT==0x80
     * 
     * DEC:
     * H_FLAG = (RESULT&0x0F)==0x0F
     * V_FLAG = RESULT==0x7F
     */
    // Incrementa un valor de 8 bits modificando los flags oportunos

    unsigned int inc8(unsigned int oper8) {
        oper8 = (oper8 + 1) & 0xff;

        sz5h3pnFlags = sz53n_addTable[oper8];

        if ((oper8 & 0x0f) == 0) {
            sz5h3pnFlags |= HALFCARRY_MASK;
        }

        if (oper8 == 0x80) {
            sz5h3pnFlags |= OVERFLOW_MASK;
        }

        flagQ = true;
        return oper8;
    }

    // Decrementa un valor de 8 bits modificando los flags oportunos

    unsigned int dec8(unsigned int oper8) {
        oper8 = (oper8 - 1) & 0xff;

        sz5h3pnFlags = sz53n_subTable[oper8];

        if ((oper8 & 0x0f) == 0x0f) {
            sz5h3pnFlags |= HALFCARRY_MASK;
        }

        if (oper8 == 0x7f) {
            sz5h3pnFlags |= OVERFLOW_MASK;
        }

        flagQ = true;
        return oper8;
    }

    // Suma con acarreo de 8 bits

    void adc(unsigned int oper8) {
        int res = regA + oper8;

        if (carryFlag) {
            res++;
        }

        carryFlag = res > 0xff;
        res &= 0xff;
        sz5h3pnFlags = sz53n_addTable[res];

        if (((regA ^ oper8 ^ res) & 0x10) != 0) {
            sz5h3pnFlags |= HALFCARRY_MASK;
        }

        if (((regA ^ ~oper8) & (regA ^ res)) > 0x7f) {
            sz5h3pnFlags |= OVERFLOW_MASK;
        }

        regA = res;
        flagQ = true;
    }

    // Suma dos operandos de 16 bits sin carry afectando a los flags

    unsigned int add16(unsigned int reg16, unsigned int oper16) {
        oper16 += reg16;

        carryFlag = oper16 > 0xffff;
        sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | ((oper16 >> 8) & FLAG_53_MASK);
        oper16 &= 0xffff;

        if ((oper16 & 0x0fff) < (reg16 & 0x0fff)) {
            sz5h3pnFlags |= HALFCARRY_MASK;
        }

        memptr = reg16 + 1;
        flagQ = true;
        return oper16;
    }

    // Suma con acarreo de 16 bits

    void adc16(unsigned int reg16) {
        int regHL = getRegHL();
        memptr = regHL + 1;

        int res = regHL + reg16;
        if (carryFlag) {
            res++;
        }

        carryFlag = res > 0xffff;
        res &= 0xffff;
        setRegHL(res);

        sz5h3pnFlags = sz53n_addTable[regH];
        if (res != 0) {
            sz5h3pnFlags &= ~ZERO_MASK;
        }

        if (((res ^ regHL ^ reg16) & 0x1000) != 0) {
            sz5h3pnFlags |= HALFCARRY_MASK;
        }

        if (((regHL ^ ~reg16) & (regHL ^ res)) > 0x7fff) {
            sz5h3pnFlags |= OVERFLOW_MASK;
        }

        flagQ = true;
    }

    // Resta con acarreo de 8 bits

    void sbc(unsigned int oper8) {
        int res = regA - oper8;

        if (carryFlag) {
            res--;
        }

        carryFlag = res < 0;
        res &= 0xff;
        sz5h3pnFlags = sz53n_subTable[res];

        if (((regA ^ oper8 ^ res) & 0x10) != 0) {
            sz5h3pnFlags |= HALFCARRY_MASK;
        }

        if (((regA ^ oper8) & (regA ^ res)) > 0x7f) {
            sz5h3pnFlags |= OVERFLOW_MASK;
        }

        regA = res;
        flagQ = true;
    }

    // Resta con acarreo de 16 bits

    void sbc16(unsigned int reg16) {
        int regHL = getRegHL();
        memptr = regHL + 1;

        int res = regHL - reg16;
        if (carryFlag) {
            res--;
        }

        carryFlag = res < 0;
        res &= 0xffff;
        setRegHL(res);

        sz5h3pnFlags = sz53n_subTable[regH];
        if (res != 0) {
            sz5h3pnFlags &= ~ZERO_MASK;
        }

        if (((res ^ regHL ^ reg16) & 0x1000) != 0) {
            sz5h3pnFlags |= HALFCARRY_MASK;
        }

        if (((regHL ^ reg16) & (regHL ^ res)) > 0x7fff) {
            sz5h3pnFlags |= OVERFLOW_MASK;
        }
        flagQ = true;
    }

    // Operación AND lógica

    void and_(unsigned int oper8) {
        regA &= oper8;
        carryFlag = false;
        sz5h3pnFlags = sz53pn_addTable[regA] | HALFCARRY_MASK;
        flagQ = true;
    }

    // Operación XOR lógica

    void xor_(unsigned int oper8) {
        regA = (regA ^ oper8) & 0xff;
        carryFlag = false;
        sz5h3pnFlags = sz53pn_addTable[regA];
        flagQ = true;
    }

    // Operación OR lógica

    void or_(unsigned int oper8) {
        regA = (regA | oper8) & 0xff;
        carryFlag = false;
        sz5h3pnFlags = sz53pn_addTable[regA];
        flagQ = true;
    }

    // Operación de comparación con el registro A
    // es como SUB, pero solo afecta a los flags
    // Los flags SIGN y ZERO se calculan a partir del resultado
    // Los flags 3 y 5 se copian desde el operando (sigh!)

    void cp(unsigned int oper8) {
        int res = regA - (oper8 & 0xff);

        carryFlag = res < 0;
        res &= 0xff;

        sz5h3pnFlags = (sz53n_addTable[oper8] & FLAG_53_MASK)
                | // No necesito preservar H, pero está a 0 en la tabla de todas formas
                (sz53n_subTable[res] & FLAG_SZHN_MASK);

        if ((res & 0x0f) > (regA & 0x0f)) {
            sz5h3pnFlags |= HALFCARRY_MASK;
        }

        if (((regA ^ oper8) & (regA ^ res)) > 0x7f) {
            sz5h3pnFlags |= OVERFLOW_MASK;
        }

        flagQ = true;
    }

    // DAA

    void daa() {
        unsigned int suma = 0;
        bool carry = carryFlag;

        if ((sz5h3pnFlags & HALFCARRY_MASK) != 0 || (regA & 0x0f) > 0x09) {
            suma = 6;
        }

        if (carry || (regA > 0x99)) {
            suma |= 0x60;
        }

        if (regA > 0x99) {
            carry = true;
        }

        carryFlag = false;
        if ((sz5h3pnFlags & ADDSUB_MASK) != 0) {
            sbc(suma);
            sz5h3pnFlags = (sz5h3pnFlags & HALFCARRY_MASK) | sz53pn_subTable[regA];
        } else {
            adc(suma);
            sz5h3pnFlags = (sz5h3pnFlags & HALFCARRY_MASK) | sz53pn_addTable[regA];
        }

        carryFlag = carry;
        // Los add/sub ya ponen el resto de los flags
        flagQ = true;
    }

    // POP

    unsigned int pop() {
        uint16_t word = Z80opsImpl.peek16(regSP);
        regSP = regSP + 2;
        return word;
    }

    // PUSH

    void push(uint16_t word) {
        regSP--;
        Z80opsImpl.poke8(regSP, word >> 8);
        regSP--;
        Z80opsImpl.poke8(regSP, word);
    }

    // LDI

    void ldi() {
        uint8_t work8 = Z80opsImpl.peek8(getRegHL());
        uint16_t regDE = getRegDE();
        Z80opsImpl.poke8(regDE, work8);
        Z80opsImpl.contendedStates(regDE, 2);
        incRegHL();
        incRegDE();
        decRegBC();
        work8 += regA;

        sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZ_MASK) | (work8 & BIT3_MASK);

        if ((work8 & ADDSUB_MASK) != 0) {
            sz5h3pnFlags |= BIT5_MASK;
        }

        if (regC != 0 || regB != 0) {
            sz5h3pnFlags |= PARITY_MASK;
        }
        flagQ = true;
    }

    // LDD

    void ldd() {
        uint8_t work8 = Z80opsImpl.peek8(getRegHL());
        uint16_t regDE = getRegDE();
        Z80opsImpl.poke8(regDE, work8);
        Z80opsImpl.contendedStates(regDE, 2);
        decRegHL();
        decRegDE();
        decRegBC();
        work8 += regA;

        sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZ_MASK) | (work8 & BIT3_MASK);

        if ((work8 & ADDSUB_MASK) != 0) {
            sz5h3pnFlags |= BIT5_MASK;
        }

        if (regC != 0 || regB != 0) {
            sz5h3pnFlags |= PARITY_MASK;
        }
        flagQ = true;
    }

    // CPI

    void cpi() {
        uint16_t regHL = getRegHL();
        uint16_t memHL = Z80opsImpl.peek8(regHL);
        bool carry = carryFlag; // lo guardo porque cp lo toca
        cp(memHL);
        carryFlag = carry;
        Z80opsImpl.contendedStates(regHL, 5);
        incRegHL();
        decRegBC();
        memHL = regA - memHL - ((sz5h3pnFlags & HALFCARRY_MASK) != 0 ? 1 : 0);
        sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHN_MASK) | (memHL & BIT3_MASK);

        if ((memHL & ADDSUB_MASK) != 0) {
            sz5h3pnFlags |= BIT5_MASK;
        }

        if (regC != 0 || regB != 0) {
            sz5h3pnFlags |= PARITY_MASK;
        }

        memptr++;
        flagQ = true;
    }

    // CPD

    void cpd() {
        uint16_t regHL = getRegHL();
        uint16_t memHL = Z80opsImpl.peek8(regHL);
        bool carry = carryFlag; // lo guardo porque cp lo toca
        cp(memHL);
        carryFlag = carry;
        Z80opsImpl.contendedStates(regHL, 5);
        decRegHL();
        decRegBC();
        memHL = regA - memHL - ((sz5h3pnFlags & HALFCARRY_MASK) != 0 ? 1 : 0);
        sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHN_MASK) | (memHL & BIT3_MASK);

        if ((memHL & ADDSUB_MASK) != 0) {
            sz5h3pnFlags |= BIT5_MASK;
        }

        if (regC != 0 || regB != 0) {
            sz5h3pnFlags |= PARITY_MASK;
        }

        memptr--;
        flagQ = true;
    }

    // INI

    void ini() {
        memptr = getRegBC();
        Z80opsImpl.contendedStates(getPairIR(), 1);
        uint8_t work8 = Z80opsImpl.inPort(memptr);
        Z80opsImpl.poke8(getRegHL(), work8);

        memptr++;
        regB = (regB - 1) & 0xff;

        incRegHL();

        sz5h3pnFlags = sz53pn_addTable[regB];
        if (work8 > 0x7f) {
            sz5h3pnFlags |= ADDSUB_MASK;
        }

        carryFlag = false;
        uint16_t tmp = work8 + ((regC + 1) & 0xff);
        if (tmp > 0xff) {
            sz5h3pnFlags |= HALFCARRY_MASK;
            carryFlag = true;
        }

        if ((sz53pn_addTable[((tmp & 0x07) ^ regB)]
                & PARITY_MASK) == PARITY_MASK) {
            sz5h3pnFlags |= PARITY_MASK;
        } else {
            sz5h3pnFlags &= ~PARITY_MASK;
        }
        flagQ = true;
    }

    // IND

    void ind() {
        memptr = getRegBC();
        Z80opsImpl.contendedStates(getPairIR(), 1);
        uint8_t work8 = Z80opsImpl.inPort(memptr);
        Z80opsImpl.poke8(getRegHL(), work8);

        memptr--;
        regB = (regB - 1) & 0xff;

        decRegHL();

        sz5h3pnFlags = sz53pn_addTable[regB];
        if (work8 > 0x7f) {
            sz5h3pnFlags |= ADDSUB_MASK;
        }

        carryFlag = false;
        uint16_t tmp = work8 + ((regC - 1) & 0xff);
        if (tmp > 0xff) {
            sz5h3pnFlags |= HALFCARRY_MASK;
            carryFlag = true;
        }

        if ((sz53pn_addTable[((tmp & 0x07) ^ regB)]
                & PARITY_MASK) == PARITY_MASK) {
            sz5h3pnFlags |= PARITY_MASK;
        } else {
            sz5h3pnFlags &= ~PARITY_MASK;
        }
        flagQ = true;
    }

    // OUTI

    void outi() {

        Z80opsImpl.contendedStates(getPairIR(), 1);

        regB = (regB - 1) & 0xff;
        memptr = getRegBC();

        uint8_t work8 = Z80opsImpl.peek8(getRegHL());
        Z80opsImpl.outPort(memptr, work8);
        memptr++;

        incRegHL();

        carryFlag = false;
        if (work8 > 0x7f) {
            sz5h3pnFlags = sz53n_subTable[regB];
        } else {
            sz5h3pnFlags = sz53n_addTable[regB];
        }

        if ((regL + work8) > 0xff) {
            sz5h3pnFlags |= HALFCARRY_MASK;
            carryFlag = true;
        }

        if ((sz53pn_addTable[(((regL + work8) & 0x07) ^ regB)]
                & PARITY_MASK) == PARITY_MASK) {
            sz5h3pnFlags |= PARITY_MASK;
        }
        flagQ = true;
    }

    // OUTD

    void outd() {

        Z80opsImpl.contendedStates(getPairIR(), 1);

        regB = (regB - 1) & 0xff;
        memptr = getRegBC();

        uint8_t work8 = Z80opsImpl.peek8(getRegHL());
        Z80opsImpl.outPort(memptr, work8);
        memptr--;

        decRegHL();

        carryFlag = false;
        if (work8 > 0x7f) {
            sz5h3pnFlags = sz53n_subTable[regB];
        } else {
            sz5h3pnFlags = sz53n_addTable[regB];
        }

        if ((regL + work8) > 0xff) {
            sz5h3pnFlags |= HALFCARRY_MASK;
            carryFlag = true;
        }

        if ((sz53pn_addTable[(((regL + work8) & 0x07) ^ regB)]
                & PARITY_MASK) == PARITY_MASK) {
            sz5h3pnFlags |= PARITY_MASK;
        }
        flagQ = true;
    }

    // Pone a 1 el Flag Z si el bit b del registro
    // r es igual a 0

    /*
     * En contra de lo que afirma el Z80-Undocumented, los bits 3 y 5 toman
     * SIEMPRE el valor de los bits correspondientes del valor a comparar para
     * las instrucciones BIT n,r. Para BIT n,(HL) toman el valor del registro
     * escondido (memptr), y para las BIT n, (IX/IY+n) toman el valor de los
     * bits superiores de la dirección indicada por IX/IY+n.
     *
     * 04/12/08 Confirmado el comentario anterior:
     *          http://scratchpad.wikia.com/wiki/Z80
     */
    void bit(unsigned int mask, unsigned int reg) {
        bool zeroFlag = (mask & reg) == 0;

        sz5h3pnFlags = (sz53n_addTable[reg] & ~FLAG_SZP_MASK) | HALFCARRY_MASK;

        if (zeroFlag) {
            sz5h3pnFlags |= (PARITY_MASK | ZERO_MASK);
        }

        if (mask == SIGN_MASK && !zeroFlag) {
            sz5h3pnFlags |= SIGN_MASK;
        }
        flagQ = true;
    }

    //Interrupción

    /* Desglose de la interrupción, según el modo:
     * IM0:
     *      M1: 7 T-Estados -> reconocer INT y decSP
     *      M2: 3 T-Estados -> escribir unsigned char alto y decSP
     *      M3: 3 T-Estados -> escribir unsigned char bajo y salto a N
     * IM1:
     *      M1: 7 T-Estados -> reconocer INT y decSP
     *      M2: 3 T-Estados -> escribir unsigned char alto PC y decSP
     *      M3: 3 T-Estados -> escribir unsigned char bajo PC y PC=0x0038
     * IM2:
     *      M1: 7 T-Estados -> reconocer INT y decSP
     *      M2: 3 T-Estados -> escribir unsigned char alto y decSP
     *      M3: 3 T-Estados -> escribir unsigned char bajo
     *      M4: 3 T-Estados -> leer unsigned char bajo del vector de INT
     *      M5: 3 T-Estados -> leer unsigned char alto y saltar a la rutina de INT
     */
    void interruption() {

        //System.out.println(String.format("INT at %d T-States", tEstados));
        //        unsigned int tmp = tEstados; // peek8 modifica los tEstados
        // Si estaba en un HALT esperando una INT, lo saca de la espera
        if (halted) {
            halted = false;
            regPC++;
        }

        klock.addTstates(7);

        regR++;
        ffIFF1 = ffIFF2 = false;
        push(regPC); // el push añadirá 6 t-estados (+contended si toca)
        if (modeINT == IntMode::IM2) {
            regPC = Z80opsImpl.peek16((regI << 8) | 0xff); // +6 t-estados
        } else {
            regPC = 0x0038;
        }
        memptr = regPC;
        //System.out.println(String.format("Coste INT: %d", tEstados-tmp));
    }

    //Interrupción NMI, no utilizado por ahora

    /* Desglose de ciclos de máquina y T-Estados
     * M1: 5 T-Estados -> extraer opcode (pá ná, es tontería) y decSP
     * M2: 3 T-Estados -> escribe unsigned char alto de PC y decSP
     * M3: 3 T-Estados -> escribe unsigned char bajo de PC y PC=0x0066
     */
    void nmi() {
        // Esta lectura consigue dos cosas:
        //      1.- La lectura del opcode del M1 que se descarta
        //      2.- Si estaba en un HALT esperando una INT, lo saca de la espera
        Z80opsImpl.fetchOpcode(regPC);
        klock.addTstates(1);
        if (halted) {
            halted = false;
            regPC++;
        }
        regR++;
        ffIFF1 = false;
        push(regPC); // 3+3 t-estados + contended si procede
        regPC = memptr = 0x0066;
    }

    bool isBreakpoint(uint16_t address) {
        return breakpointAt[address & 0xffff];
    }

    void setBreakpoint(uint16_t address, bool state) {
        breakpointAt[address] = state;
    }

    void resetBreakpoints() {
        // d6
        for (int i = 0; i < 0x10000; i++) {
            breakpointAt[i] = false;
        }
    }

    void execute() {
        //         cout << "Execute at " << regPC << endl;
        // Primero se comprueba NMI
        if (activeNMI) {
            activeNMI = false;
            lastFlagQ = false;
            nmi();
        }

        // Ahora se comprueba si al final de la instrucción anterior se
        // encontró una interrupción enmascarable y, de ser así, se procesa.
        if (activeINT) {
            if (ffIFF1 && !pendingEI) {
                lastFlagQ = false;
                interruption();
            }
        }

        //         cout << "Checking for breapoint" << endl;
        if (breakpointAt[regPC]) {
            // d6
            //cout << "Breakpoint at" << regPC << endl;
            Z80opsImpl.breakpoint();
        }

        regR++;
        opCode = Z80opsImpl.fetchOpcode(regPC);
        regPC++;

        flagQ = false;

        decodeOpcode(opCode);

        lastFlagQ = flagQ;

        // Si está pendiente la activación de la interrupciones y el
        // código que se acaba de ejecutar no es el propio EI
        if (pendingEI && opCode != 0xFB) {
            pendingEI = false;
        }

        if (execDone) {
            Z80opsImpl.execDone();
        }
    }

    /* Los tEstados transcurridos se calculan teniendo en cuenta el número de
     * ciclos de máquina reales que se ejecutan. Esa es la única forma de poder
     * simular la contended memory del Spectrum.
     */
    void execute(unsigned int statesLimit) {

        while (klock.getTstates() < statesLimit) {
            execute();
            //            // Primero se comprueba NMI
            //            if (activeNMI) {
            //                activeNMI = false;
            //                lastFlagQ = false;
            //                nmi();
            //                continue;
            //            }
            //
            //            // Ahora se comprueba si al final de la instrucción anterior se
            //            // encontró una interrupción enmascarable y, de ser así, se procesa.
            //            if (activeINT) {
            //                if (ffIFF1 && !pendingEI) {
            //                    lastFlagQ = false;
            //                    interruption();
            //                }
            //            }
            //
            //            regR++;
            //            opCode = MemIoImpl.fetchOpcode(regPC);
            //            
            //            if (breakpointAt[regPC]) {
            //                opCode = NotifyImpl.atAddress(regPC, opCode);
            //            }
            //            
            //            regPC = (regPC + 1) & 0xffff;
            //
            //            flagQ = false;
            //            
            //            decodeOpcode(opCode);
            //            
            //            lastFlagQ = flagQ;
            //
            //            // Si está pendiente la activación de la interrupciones y el
            //            // código que se acaba de ejecutar no es el propio EI
            //            if (pendingEI && opCode != 0xFB) {
            //                pendingEI = false;
            //            }
            //
            //            if (execDone) {
            //                NotifyImpl.execDone();
            //            }

        } /* del while */
    }

    void decodeOpcode(uint8_t opCode) {

        switch (opCode) {
                //            case 0x00:       /* NOP */
                //                break;
            case 0x01:
            { /* LD BC,nn */
                setRegBC(Z80opsImpl.peek16(regPC));
                regPC = regPC + 2;
                break;
            }
            case 0x02:
            { /* LD (BC),A */
                Z80opsImpl.poke8(getRegBC(), regA);
                memptr = (regA << 8) | ((regC + 1) & 0xff);
                break;
            }
            case 0x03:
            { /* INC BC */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                incRegBC();
                break;
            }
            case 0x04:
            { /* INC B */
                regB = inc8(regB);
                break;
            }
            case 0x05:
            { /* DEC B */
                regB = dec8(regB);
                break;
            }
            case 0x06:
            { /* LD B,n */
                regB = Z80opsImpl.peek8(regPC);
                regPC++;
                break;
            }
            case 0x07:
            { /* RLCA */
                carryFlag = (regA > 0x7f);
                regA = (regA << 1) & 0xff;
                if (carryFlag) {
                    regA |= CARRY_MASK;
                }
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
                flagQ = true;
                break;
            }
            case 0x08:
            { /* EX AF,AF' */
                unsigned int work8 = regA;
                regA = regAx;
                regAx = work8;

                work8 = getFlags();
                setFlags(regFx);
                regFx = work8;
                break;
            }
            case 0x09:
            { /* ADD HL,BC */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                setRegHL(add16(getRegHL(), getRegBC()));
                break;
            }
            case 0x0A:
            { /* LD A,(BC) */
                memptr = getRegBC();
                regA = Z80opsImpl.peek8(memptr++);
                break;
            }
            case 0x0B:
            { /* DEC BC */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                decRegBC();
                break;
            }
            case 0x0C:
            { /* INC C */
                regC = inc8(regC);
                break;
            }
            case 0x0D:
            { /* DEC C */
                regC = dec8(regC);
                break;
            }
            case 0x0E:
            { /* LD C,n */
                regC = Z80opsImpl.peek8(regPC);
                regPC++;
                break;
            }
            case 0x0F:
            { /* RRCA */
                carryFlag = (regA & CARRY_MASK) != 0;
                regA >>= 1;
                if (carryFlag) {
                    regA |= SIGN_MASK;
                }
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
                flagQ = true;
                break;
            }
            case 0x10:
            { /* DJNZ e */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                unsigned char offset = (unsigned char) Z80opsImpl.peek8(regPC);
                regB--;
                if (regB != 0) {
                    regB &= 0xff;
                    Z80opsImpl.contendedStates(regPC, 5);
                    regPC = memptr = regPC + offset + 1;
                } else {
                    regPC++;
                }
                break;
            }
            case 0x11:
            { /* LD DE,nn */
                setRegDE(Z80opsImpl.peek16(regPC));
                regPC = regPC + 2;
                break;
            }
            case 0x12:
            { /* LD (DE),A */
                Z80opsImpl.poke8(getRegDE(), regA);
                memptr = (regA << 8) | ((regE + 1) & 0xff);
                break;
            }
            case 0x13:
            { /* INC DE */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                incRegDE();
                break;
            }
            case 0x14:
            { /* INC D */
                regD = inc8(regD);
                break;
            }
            case 0x15:
            { /* DEC D */
                regD = dec8(regD);
                break;
            }
            case 0x16:
            { /* LD D,n */
                regD = Z80opsImpl.peek8(regPC);
                regPC++;
                break;
            }
            case 0x17:
            { /* RLA */
                bool oldCarry = carryFlag;
                carryFlag = (regA > 0x7f);
                regA = (regA << 1) & 0xff;
                if (oldCarry) {
                    regA |= CARRY_MASK;
                }
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
                flagQ = true;
                break;
            }
            case 0x18:
            { /* JR e */
                unsigned char offset = (unsigned char) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                regPC = memptr = regPC + offset + 1;
                break;
            }
            case 0x19:
            { /* ADD HL,DE */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                setRegHL(add16(getRegHL(), getRegDE()));
                break;
            }
            case 0x1A:
            { /* LD A,(DE) */
                memptr = getRegDE();
                regA = Z80opsImpl.peek8(memptr++);
                break;
            }
            case 0x1B:
            { /* DEC DE */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                decRegDE();
                break;
            }
            case 0x1C:
            { /* INC E */
                regE = inc8(regE);
                break;
            }
            case 0x1D:
            { /* DEC E */
                regE = dec8(regE);
                break;
            }
            case 0x1E:
            { /* LD E,n */
                regE = Z80opsImpl.peek8(regPC);
                regPC++;
                break;
            }
            case 0x1F:
            { /* RRA */
                bool oldCarry = carryFlag;
                carryFlag = (regA & CARRY_MASK) != 0;
                regA >>= 1;
                if (oldCarry) {
                    regA |= SIGN_MASK;
                }
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (regA & FLAG_53_MASK);
                flagQ = true;
                break;
            }
            case 0x20:
            { /* JR NZ,e */
                unsigned char offset = (unsigned char) Z80opsImpl.peek8(regPC);
                if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                    Z80opsImpl.contendedStates(regPC, 5);
                    regPC += offset;
                    memptr = regPC + 1;
                }
                regPC++;
                break;
            }
            case 0x21:
            { /* LD HL,nn */
                setRegHL(Z80opsImpl.peek16(regPC));
                regPC = regPC + 2;
                break;
            }
            case 0x22:
            { /* LD (nn),HL */
                memptr = Z80opsImpl.peek16(regPC);
                Z80opsImpl.poke16(memptr++, getRegHL());
                regPC = regPC + 2;
                break;
            }
            case 0x23:
            { /* INC HL */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                incRegHL();
                break;
            }
            case 0x24:
            { /* INC H */
                regH = inc8(regH);
                break;
            }
            case 0x25:
            { /* DEC H */
                regH = dec8(regH);
                break;
            }
            case 0x26:
            { /* LD H,n */
                regH = Z80opsImpl.peek8(regPC);
                regPC++;
                break;
            }
            case 0x27:
            { /* DAA */
                daa();
                break;
            }
            case 0x28:
            { /* JR Z,e */
                unsigned char offset = (unsigned char) Z80opsImpl.peek8(regPC);
                if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                    Z80opsImpl.contendedStates(regPC, 5);
                    regPC += offset;
                    memptr = regPC + 1;
                }
                regPC++;
                break;
            }
            case 0x29:
            { /* ADD HL,HL */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                unsigned int work16 = getRegHL();
                setRegHL(add16(work16, work16));
                break;
            }
            case 0x2A:
            { /* LD HL,(nn) */
                memptr = Z80opsImpl.peek16(regPC);
                setRegHL(Z80opsImpl.peek16(memptr++));
                regPC = regPC + 2;
                break;
            }
            case 0x2B:
            { /* DEC HL */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                decRegHL();
                break;
            }
            case 0x2C:
            { /* INC L */
                regL = inc8(regL);
                break;
            }
            case 0x2D:
            { /* DEC L */
                regL = dec8(regL);
                break;
            }
            case 0x2E:
            { /* LD L,n */
                regL = Z80opsImpl.peek8(regPC);
                regPC++;
                break;
            }
            case 0x2F:
            { /* CPL */
                regA ^= 0xff;
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | HALFCARRY_MASK
                        | (regA & FLAG_53_MASK) | ADDSUB_MASK;
                flagQ = true;
                break;
            }
            case 0x30:
            { /* JR NC,e */
                unsigned char offset = (unsigned char) Z80opsImpl.peek8(regPC);
                if (!carryFlag) {
                    Z80opsImpl.contendedStates(regPC, 5);
                    regPC += offset;
                    memptr = regPC + 1;
                }
                regPC++;
                break;
            }
            case 0x31:
            { /* LD SP,nn */
                regSP = Z80opsImpl.peek16(regPC);
                regPC = regPC + 2;
                break;
            }
            case 0x32:
            { /* LD (nn),A */
                memptr = Z80opsImpl.peek16(regPC);
                Z80opsImpl.poke8(memptr, regA);
                memptr = (regA << 8) | ((memptr + 1) & 0xff);
                regPC = regPC + 2;
                break;
            }
            case 0x33:
            { /* INC SP */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                regSP++;
                break;
            }
            case 0x34:
            { /* INC (HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = inc8(Z80opsImpl.peek8(work16));
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x35:
            { /* DEC (HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = dec8(Z80opsImpl.peek8(work16));
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x36:
            { /* LD (HL),n */
                Z80opsImpl.poke8(getRegHL(), Z80opsImpl.peek8(regPC));
                regPC++;
                break;
            }
            case 0x37:
            { /* SCF */
                unsigned int regQ = lastFlagQ ? sz5h3pnFlags : 0;
                carryFlag = true;
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (((regQ ^ sz5h3pnFlags) | regA) & FLAG_53_MASK);
                flagQ = true;
                break;
            }
            case 0x38:
            { /* JR C,e */
                unsigned char offset = (unsigned char) Z80opsImpl.peek8(regPC);
                if (carryFlag) {
                    Z80opsImpl.contendedStates(regPC, 5);
                    regPC += offset;
                    memptr = regPC + 1;
                }
                regPC++;
                break;
            }
            case 0x39:
            { /* ADD HL,SP */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                setRegHL(add16(getRegHL(), regSP));
                break;
            }
            case 0x3A:
            { /* LD A,(nn) */
                memptr = Z80opsImpl.peek16(regPC);
                regA = Z80opsImpl.peek8(memptr++);
                regPC = regPC + 2;
                break;
            }
            case 0x3B:
            { /* DEC SP */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                regSP--;
                break;
            }
            case 0x3C:
            { /* INC A */
                regA = inc8(regA);
                break;
            }
            case 0x3D:
            { /* DEC A */
                regA = dec8(regA);
                break;
            }
            case 0x3E:
            { /* LD A,n */
                regA = Z80opsImpl.peek8(regPC);
                regPC++;
                break;
            }
            case 0x3F:
            { /* CCF */
                unsigned int regQ = lastFlagQ ? sz5h3pnFlags : 0;
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZP_MASK) | (((regQ ^ sz5h3pnFlags) | regA) & FLAG_53_MASK);
                if (carryFlag) {
                    sz5h3pnFlags |= HALFCARRY_MASK;
                }
                carryFlag = !carryFlag;
                flagQ = true;
                break;
            }
                //            case 0x40: {     /* LD B,B */
                //                break;
                //            }
            case 0x41:
            { /* LD B,C */
                regB = regC;
                break;
            }
            case 0x42:
            { /* LD B,D */
                regB = regD;
                break;
            }
            case 0x43:
            { /* LD B,E */
                regB = regE;
                break;
            }
            case 0x44:
            { /* LD B,H */
                regB = regH;
                break;
            }
            case 0x45:
            { /* LD B,L */
                regB = regL;
                break;
            }
            case 0x46:
            { /* LD B,(HL) */
                regB = Z80opsImpl.peek8(getRegHL());
                break;
            }
            case 0x47:
            { /* LD B,A */
                regB = regA;
                break;
            }
            case 0x48:
            { /* LD C,B */
                regC = regB;
                break;
            }
                //            case 0x49: {     /* LD C,C */
                //                break;
                //            }
            case 0x4A:
            { /* LD C,D */
                regC = regD;
                break;
            }
            case 0x4B:
            { /* LD C,E */
                regC = regE;
                break;
            }
            case 0x4C:
            { /* LD C,H */
                regC = regH;
                break;
            }
            case 0x4D:
            { /* LD C,L */
                regC = regL;
                break;
            }
            case 0x4E:
            { /* LD C,(HL) */
                regC = Z80opsImpl.peek8(getRegHL());
                break;
            }
            case 0x4F:
            { /* LD C,A */
                regC = regA;
                break;
            }
            case 0x50:
            { /* LD D,B */
                regD = regB;
                break;
            }
            case 0x51:
            { /* LD D,C */
                regD = regC;
                break;
            }
                //            case 0x52: {     /* LD D,D */
                //                break;
                //            }
            case 0x53:
            { /* LD D,E */
                regD = regE;
                break;
            }
            case 0x54:
            { /* LD D,H */
                regD = regH;
                break;
            }
            case 0x55:
            { /* LD D,L */
                regD = regL;
                break;
            }
            case 0x56:
            { /* LD D,(HL) */
                regD = Z80opsImpl.peek8(getRegHL());
                break;
            }
            case 0x57:
            { /* LD D,A */
                regD = regA;
                break;
            }
            case 0x58:
            { /* LD E,B */
                regE = regB;
                break;
            }
            case 0x59:
            { /* LD E,C */
                regE = regC;
                break;
            }
            case 0x5A:
            { /* LD E,D */
                regE = regD;
                break;
            }
                //            case 0x5B: {     /* LD E,E */
                //                break;
                //            }
            case 0x5C:
            { /* LD E,H */
                regE = regH;
                break;
            }
            case 0x5D:
            { /* LD E,L */
                regE = regL;
                break;
            }
            case 0x5E:
            { /* LD E,(HL) */
                regE = Z80opsImpl.peek8(getRegHL());
                break;
            }
            case 0x5F:
            { /* LD E,A */
                regE = regA;
                break;
            }
            case 0x60:
            { /* LD H,B */
                regH = regB;
                break;
            }
            case 0x61:
            { /* LD H,C */
                regH = regC;
                break;
            }
            case 0x62:
            { /* LD H,D */
                regH = regD;
                break;
            }
            case 0x63:
            { /* LD H,E */
                regH = regE;
                break;
            }
                //            case 0x64: {     /* LD H,H */
                //                break;
                //            }
            case 0x65:
            { /* LD H,L */
                regH = regL;
                break;
            }
            case 0x66:
            { /* LD H,(HL) */
                regH = Z80opsImpl.peek8(getRegHL());
                break;
            }
            case 0x67:
            { /* LD H,A */
                regH = regA;
                break;
            }
            case 0x68:
            { /* LD L,B */
                regL = regB;
                break;
            }
            case 0x69:
            { /* LD L,C */
                regL = regC;
                break;
            }
            case 0x6A:
            { /* LD L,D */
                regL = regD;
                break;
            }
            case 0x6B:
            { /* LD L,E */
                regL = regE;
                break;
            }
            case 0x6C:
            { /* LD L,H */
                regL = regH;
                break;
            }
                //            case 0x6D: {     /* LD L,L */
                //                break;
                //            }
            case 0x6E:
            { /* LD L,(HL) */
                regL = Z80opsImpl.peek8(getRegHL());
                break;
            }
            case 0x6F:
            { /* LD L,A */
                regL = regA;
                break;
            }
            case 0x70:
            { /* LD (HL),B */
                Z80opsImpl.poke8(getRegHL(), regB);
                break;
            }
            case 0x71:
            { /* LD (HL),C */
                Z80opsImpl.poke8(getRegHL(), regC);
                break;
            }
            case 0x72:
            { /* LD (HL),D */
                Z80opsImpl.poke8(getRegHL(), regD);
                break;
            }
            case 0x73:
            { /* LD (HL),E */
                Z80opsImpl.poke8(getRegHL(), regE);
                break;
            }
            case 0x74:
            { /* LD (HL),H */
                Z80opsImpl.poke8(getRegHL(), regH);
                break;
            }
            case 0x75:
            { /* LD (HL),L */
                Z80opsImpl.poke8(getRegHL(), regL);
                break;
            }
            case 0x76:
            { /* HALT */
                regPC--;
                halted = true;
                break;
            }
            case 0x77:
            { /* LD (HL),A */
                Z80opsImpl.poke8(getRegHL(), regA);
                break;
            }
            case 0x78:
            { /* LD A,B */
                regA = regB;
                break;
            }
            case 0x79:
            { /* LD A,C */
                regA = regC;
                break;
            }
            case 0x7A:
            { /* LD A,D */
                regA = regD;
                break;
            }
            case 0x7B:
            { /* LD A,E */
                regA = regE;
                break;
            }
            case 0x7C:
            { /* LD A,H */
                regA = regH;
                break;
            }
            case 0x7D:
            { /* LD A,L */
                regA = regL;
                break;
            }
            case 0x7E:
            { /* LD A,(HL) */
                regA = Z80opsImpl.peek8(getRegHL());
                break;
            }
                //            case 0x7F: {     /* LD A,A */
                //                break;
                //            }
            case 0x80:
            { /* ADD A,B */
                carryFlag = false;
                adc(regB);
                break;
            }
            case 0x81:
            { /* ADD A,C */
                carryFlag = false;
                adc(regC);
                break;
            }
            case 0x82:
            { /* ADD A,D */
                carryFlag = false;
                adc(regD);
                break;
            }
            case 0x83:
            { /* ADD A,E */
                carryFlag = false;
                adc(regE);
                break;
            }
            case 0x84:
            { /* ADD A,H */
                carryFlag = false;
                adc(regH);
                break;
            }
            case 0x85:
            { /* ADD A,L */
                carryFlag = false;
                adc(regL);
                break;
            }
            case 0x86:
            { /* ADD A,(HL) */
                carryFlag = false;
                adc(Z80opsImpl.peek8(getRegHL()));
                break;
            }
            case 0x87:
            { /* ADD A,A */
                carryFlag = false;
                adc(regA);
                break;
            }
            case 0x88:
            { /* ADC A,B */
                adc(regB);
                break;
            }
            case 0x89:
            { /* ADC A,C */
                adc(regC);
                break;
            }
            case 0x8A:
            { /* ADC A,D */
                adc(regD);
                break;
            }
            case 0x8B:
            { /* ADC A,E */
                adc(regE);
                break;
            }
            case 0x8C:
            { /* ADC A,H */
                adc(regH);
                break;
            }
            case 0x8D:
            { /* ADC A,L */
                adc(regL);
                break;
            }
            case 0x8E:
            { /* ADC A,(HL) */
                adc(Z80opsImpl.peek8(getRegHL()));
                break;
            }
            case 0x8F:
            { /* ADC A,A */
                adc(regA);
                break;
            }
            case 0x90:
            { /* SUB B */
                carryFlag = false;
                sbc(regB);
                break;
            }
            case 0x91:
            { /* SUB C */
                carryFlag = false;
                sbc(regC);
                break;
            }
            case 0x92:
            { /* SUB D */
                carryFlag = false;
                sbc(regD);
                break;
            }
            case 0x93:
            { /* SUB E */
                carryFlag = false;
                sbc(regE);
                break;
            }
            case 0x94:
            { /* SUB H */
                carryFlag = false;
                sbc(regH);
                break;
            }
            case 0x95:
            { /* SUB L */
                carryFlag = false;
                sbc(regL);
                break;
            }
            case 0x96:
            { /* SUB (HL) */
                carryFlag = false;
                sbc(Z80opsImpl.peek8(getRegHL()));
                break;
            }
            case 0x97:
            { /* SUB A */
                carryFlag = false;
                sbc(regA);
                break;
            }
            case 0x98:
            { /* SBC A,B */
                sbc(regB);
                break;
            }
            case 0x99:
            { /* SBC A,C */
                sbc(regC);
                break;
            }
            case 0x9A:
            { /* SBC A,D */
                sbc(regD);
                break;
            }
            case 0x9B:
            { /* SBC A,E */
                sbc(regE);
                break;
            }
            case 0x9C:
            { /* SBC A,H */
                sbc(regH);
                break;
            }
            case 0x9D:
            { /* SBC A,L */
                sbc(regL);
                break;
            }
            case 0x9E:
            { /* SBC A,(HL) */
                sbc(Z80opsImpl.peek8(getRegHL()));
                break;
            }
            case 0x9F:
            { /* SBC A,A */
                sbc(regA);
                break;
            }
            case 0xA0:
            { /* AND B */
                and_(regB);
                break;
            }
            case 0xA1:
            { /* AND C */
                and_(regC);
                break;
            }
            case 0xA2:
            { /* AND D */
                and_(regD);
                break;
            }
            case 0xA3:
            { /* AND E */
                and_(regE);
                break;
            }
            case 0xA4:
            { /* AND H */
                and_(regH);
                break;
            }
            case 0xA5:
            { /* AND L */
                and_(regL);
                break;
            }
            case 0xA6:
            { /* AND (HL) */
                and_(Z80opsImpl.peek8(getRegHL()));
                break;
            }
            case 0xA7:
            { /* AND A */
                and_(regA);
                break;
            }
            case 0xA8:
            { /* XOR B */
                xor_(regB);
                break;
            }
            case 0xA9:
            { /* XOR C */
                xor_(regC);
                break;
            }
            case 0xAA:
            { /* XOR D */
                xor_(regD);
                break;
            }
            case 0xAB:
            { /* XOR E */
                xor_(regE);
                break;
            }
            case 0xAC:
            { /* XOR H */
                xor_(regH);
                break;
            }
            case 0xAD:
            { /* XOR L */
                xor_(regL);
                break;
            }
            case 0xAE:
            { /* XOR (HL) */
                xor_(Z80opsImpl.peek8(getRegHL()));
                break;
            }
            case 0xAF:
            { /* XOR A */
                xor_(regA);
                break;
            }
            case 0xB0:
            { /* OR B */
                or_(regB);
                break;
            }
            case 0xB1:
            { /* OR C */
                or_(regC);
                break;
            }
            case 0xB2:
            { /* OR D */
                or_(regD);
                break;
            }
            case 0xB3:
            { /* OR E */
                or_(regE);
                break;
            }
            case 0xB4:
            { /* OR H */
                or_(regH);
                break;
            }
            case 0xB5:
            { /* OR L */
                or_(regL);
                break;
            }
            case 0xB6:
            { /* OR (HL) */
                or_(Z80opsImpl.peek8(getRegHL()));
                break;
            }
            case 0xB7:
            { /* OR A */
                or_(regA);
                break;
            }
            case 0xB8:
            { /* CP B */
                cp(regB);
                break;
            }
            case 0xB9:
            { /* CP C */
                cp(regC);
                break;
            }
            case 0xBA:
            { /* CP D */
                cp(regD);
                break;
            }
            case 0xBB:
            { /* CP E */
                cp(regE);
                break;
            }
            case 0xBC:
            { /* CP H */
                cp(regH);
                break;
            }
            case 0xBD:
            { /* CP L */
                cp(regL);
                break;
            }
            case 0xBE:
            { /* CP (HL) */
                cp(Z80opsImpl.peek8(getRegHL()));
                break;
            }
            case 0xBF:
            { /* CP A */
                cp(regA);
                break;
            }
            case 0xC0:
            { /* RET NZ */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                    regPC = memptr = pop();
                }
                break;
            }
            case 0xC1:
            { /* POP BC */
                setRegBC(pop());
                break;
            }
            case 0xC2:
            { /* JP NZ,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            }
            case 0xC3:
            { /* JP nn */
                memptr = regPC = Z80opsImpl.peek16(regPC);
                break;
            }
            case 0xC4:
            { /* CALL NZ,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if ((sz5h3pnFlags & ZERO_MASK) == 0) {
                    Z80opsImpl.contendedStates(regPC + 1, 1);
                    push(regPC + 2);
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            }
            case 0xC5:
            { /* PUSH BC */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(getRegBC());
                break;
            }
            case 0xC6:
            { /* ADD A,n */
                carryFlag = false;
                adc(Z80opsImpl.peek8(regPC));
                regPC++;
                break;
            }
            case 0xC7:
            { /* RST 00H */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(regPC);
                regPC = memptr = 0x00;
                break;
            }
            case 0xC8:
            { /* RET Z */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                    regPC = memptr = pop();
                }
                break;
            }
            case 0xC9:
            { /* RET */
                regPC = memptr = pop();
                break;
            }
            case 0xCA:
            { /* JP Z,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            }
            case 0xCB:
            { /* Subconjunto de instrucciones */
                decodeCB();
                break;
            }
            case 0xCC:
            { /* CALL Z,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if ((sz5h3pnFlags & ZERO_MASK) != 0) {
                    Z80opsImpl.contendedStates(regPC + 1, 1);
                    push(regPC + 2);
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            }
            case 0xCD:
            { /* CALL nn */
                memptr = Z80opsImpl.peek16(regPC);
                Z80opsImpl.contendedStates(regPC + 1, 1);
                push(regPC + 2);
                regPC = memptr;
                break;
            }
            case 0xCE:
            { /* ADC A,n */
                adc(Z80opsImpl.peek8(regPC));
                regPC++;
                break;
            }
            case 0xCF:
            { /* RST 08H */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(regPC);
                regPC = memptr = 0x08;
                break;
            }
            case 0xD0:
            { /* RET NC */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                if (!carryFlag) {
                    regPC = memptr = pop();
                }
                break;
            }
            case 0xD1:
            { /* POP DE */
                setRegDE(pop());
                break;
            }
            case 0xD2:
            { /* JP NC,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if (!carryFlag) {
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            }
            case 0xD3:
            { /* OUT (n),A */
                unsigned int work8 = Z80opsImpl.peek8(regPC);
                memptr = regA << 8;
                Z80opsImpl.outPort(memptr | work8, regA);
                memptr |= ((work8 + 1) & 0xff);
                regPC++;
                break;
            }
            case 0xD4:
            { /* CALL NC,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if (!carryFlag) {
                    Z80opsImpl.contendedStates(regPC + 1, 1);
                    push(regPC + 2);
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            }
            case 0xD5:
            { /* PUSH DE */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(getRegDE());
                break;
            }
            case 0xD6:
            { /* SUB n */
                carryFlag = false;
                sbc(Z80opsImpl.peek8(regPC));
                regPC++;
                break;
            }
            case 0xD7:
            { /* RST 10H */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(regPC);
                regPC = memptr = 0x10;
                break;
            }
            case 0xD8:
            { /* RET C */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                if (carryFlag) {
                    regPC = memptr = pop();
                }
                break;
            }
            case 0xD9:
            { /* EXX */
                unsigned int work8 = regB;
                regB = regBx;
                regBx = work8;

                work8 = regC;
                regC = regCx;
                regCx = work8;

                work8 = regD;
                regD = regDx;
                regDx = work8;

                work8 = regE;
                regE = regEx;
                regEx = work8;

                work8 = regH;
                regH = regHx;
                regHx = work8;

                work8 = regL;
                regL = regLx;
                regLx = work8;
                break;
            }
            case 0xDA:
            { /* JP C,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if (carryFlag) {
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            }
            case 0xDB:
            { /* IN A,(n) */
                memptr = (regA << 8) | Z80opsImpl.peek8(regPC);
                regA = Z80opsImpl.inPort(memptr++);
                regPC++;
                break;
            }
            case 0xDC:
            { /* CALL C,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if (carryFlag) {
                    Z80opsImpl.contendedStates(regPC + 1, 1);
                    push(regPC + 2);
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            }
            case 0xDD:
            { /* Subconjunto de instrucciones */
                regIX = decodeDDFD(regIX);
                break;
            }
            case 0xDE:
            { /* SBC A,n */
                sbc(Z80opsImpl.peek8(regPC));
                regPC++;
                break;
            }
            case 0xDF:
            { /* RST 18H */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(regPC);
                regPC = memptr = 0x18;
                break;
            }
            case 0xE0: /* RET PO */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                    regPC = memptr = pop();
                }
                break;
            case 0xE1: /* POP HL */
                setRegHL(pop());
                break;
            case 0xE2: /* JP PO,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            case 0xE3:
            { /* EX (SP),HL */
                // Instrucción de ejecución sutil.
                uint16_t work16 = regH;
                uint8_t work8 = regL;
                setRegHL(Z80opsImpl.peek16(regSP));
                Z80opsImpl.contendedStates(regSP + 1, 1);
                // No se usa poke16 porque el Z80 escribe los bytes AL REVES
                Z80opsImpl.poke8(regSP + 1, work16);
                Z80opsImpl.poke8(regSP, work8);
                Z80opsImpl.contendedStates(regSP, 2);
                memptr = getRegHL();
                break;
            }
            case 0xE4: /* CALL PO,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if ((sz5h3pnFlags & PARITY_MASK) == 0) {
                    Z80opsImpl.contendedStates(regPC + 1, 1);
                    push(regPC + 2);
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            case 0xE5: /* PUSH HL */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(getRegHL());
                break;
            case 0xE6: /* AND n */
                and_(Z80opsImpl.peek8(regPC));
                regPC++;
                break;
            case 0xE7: /* RST 20H */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(regPC);
                regPC = memptr = 0x20;
                break;
            case 0xE8: /* RET PE */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                    regPC = memptr = pop();
                }
                break;
            case 0xE9: /* JP (HL) */
                regPC = getRegHL();
                break;
            case 0xEA: /* JP PE,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            case 0xEB:
            { /* EX DE,HL */
                unsigned int work8 = regH;
                regH = regD;
                regD = work8;

                work8 = regL;
                regL = regE;
                regE = work8;
                break;
            }
            case 0xEC: /* CALL PE,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if ((sz5h3pnFlags & PARITY_MASK) != 0) {
                    Z80opsImpl.contendedStates(regPC + 1, 1);
                    push(regPC + 2);
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            case 0xED: /*Subconjunto de instrucciones*/
                decodeED();
                break;
            case 0xEE: /* XOR n */
                xor_(Z80opsImpl.peek8(regPC));
                regPC++;
                break;
            case 0xEF: /* RST 28H */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(regPC);
                regPC = memptr = 0x28;
                break;
            case 0xF0: /* RET P */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                if (sz5h3pnFlags < SIGN_MASK) {
                    regPC = memptr = pop();
                }
                break;
            case 0xF1: /* POP AF */
                setRegAF(pop());
                break;
            case 0xF2: /* JP P,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if (sz5h3pnFlags < SIGN_MASK) {
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            case 0xF3: /* DI */
                ffIFF1 = ffIFF2 = false;
                break;
            case 0xF4: /* CALL P,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if (sz5h3pnFlags < SIGN_MASK) {
                    Z80opsImpl.contendedStates(regPC + 1, 1);
                    push(regPC + 2);
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            case 0xF5: /* PUSH AF */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(getRegAF());
                break;
            case 0xF6: /* OR n */
                or_(Z80opsImpl.peek8(regPC));
                regPC++;
                break;
            case 0xF7: /* RST 30H */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(regPC);
                regPC = memptr = 0x30;
                break;
            case 0xF8: /* RET M */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                if (sz5h3pnFlags > 0x7f) {
                    regPC = memptr = pop();
                }
                break;
            case 0xF9: /* LD SP,HL */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                regSP = getRegHL();
                break;
            case 0xFA: /* JP M,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if (sz5h3pnFlags > 0x7f) {
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            case 0xFB: /* EI */
                ffIFF1 = ffIFF2 = true;
                pendingEI = true;
                break;
            case 0xFC: /* CALL M,nn */
                memptr = Z80opsImpl.peek16(regPC);
                if (sz5h3pnFlags > 0x7f) {
                    Z80opsImpl.contendedStates(regPC + 1, 1);
                    push(regPC + 2);
                    regPC = memptr;
                    break;
                }
                regPC = regPC + 2;
                break;
            case 0xFD: /* Subconjunto de instrucciones */
                regIY = decodeDDFD(regIY);
                break;
            case 0xFE: /* CP n */
                cp(Z80opsImpl.peek8(regPC));
                regPC++;
                break;
            case 0xFF: /* RST 38H */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(regPC);
                regPC = memptr = 0x38;
        } /* del switch( codigo ) */
    }

    //Subconjunto de instrucciones 0xCB

    void decodeCB() {

        regR++;
        opCode = Z80opsImpl.fetchOpcode(regPC);
        regPC++;

        switch (opCode) {
            case 0x00:
            { /* RLC B */
                regB = rlc(regB);
                break;
            }
            case 0x01:
            { /* RLC C */
                regC = rlc(regC);
                break;
            }
            case 0x02:
            { /* RLC D */
                regD = rlc(regD);
                break;
            }
            case 0x03:
            { /* RLC E */
                regE = rlc(regE);
                break;
            }
            case 0x04:
            { /* RLC H */
                regH = rlc(regH);
                break;
            }
            case 0x05:
            { /* RLC L */
                regL = rlc(regL);
                break;
            }
            case 0x06:
            { /* RLC (HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = rlc(Z80opsImpl.peek8(work16));
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x07:
            { /* RLC A */
                regA = rlc(regA);
                break;
            }
            case 0x08:
            { /* RRC B */
                regB = rrc(regB);
                break;
            }
            case 0x09:
            { /* RRC C */
                regC = rrc(regC);
                break;
            }
            case 0x0A:
            { /* RRC D */
                regD = rrc(regD);
                break;
            }
            case 0x0B:
            { /* RRC E */
                regE = rrc(regE);
                break;
            }
            case 0x0C:
            { /* RRC H */
                regH = rrc(regH);
                break;
            }
            case 0x0D:
            { /* RRC L */
                regL = rrc(regL);
                break;
            }
            case 0x0E:
            { /* RRC (HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = rrc(Z80opsImpl.peek8(work16));
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x0F:
            { /* RRC A */
                regA = rrc(regA);
                break;
            }
            case 0x10:
            { /* RL B */
                regB = rl(regB);
                break;
            }
            case 0x11:
            { /* RL C */
                regC = rl(regC);
                break;
            }
            case 0x12:
            { /* RL D */
                regD = rl(regD);
                break;
            }
            case 0x13:
            { /* RL E */
                regE = rl(regE);
                break;
            }
            case 0x14:
            { /* RL H */
                regH = rl(regH);
                break;
            }
            case 0x15:
            { /* RL L */
                regL = rl(regL);
                break;
            }
            case 0x16:
            { /* RL (HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = rl(Z80opsImpl.peek8(work16));
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x17:
            { /* RL A */
                regA = rl(regA);
                break;
            }
            case 0x18:
            { /* RR B */
                regB = rr(regB);
                break;
            }
            case 0x19:
            { /* RR C */
                regC = rr(regC);
                break;
            }
            case 0x1A:
            { /* RR D */
                regD = rr(regD);
                break;
            }
            case 0x1B:
            { /* RR E */
                regE = rr(regE);
                break;
            }
            case 0x1C:
            { /*RR H*/
                regH = rr(regH);
                break;
            }
            case 0x1D:
            { /* RR L */
                regL = rr(regL);
                break;
            }
            case 0x1E:
            { /* RR (HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = rr(Z80opsImpl.peek8(work16));
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x1F:
            { /* RR A */
                regA = rr(regA);
                break;
            }
            case 0x20:
            { /* SLA B */
                regB = sla(regB);
                break;
            }
            case 0x21:
            { /* SLA C */
                regC = sla(regC);
                break;
            }
            case 0x22:
            { /* SLA D */
                regD = sla(regD);
                break;
            }
            case 0x23:
            { /* SLA E */
                regE = sla(regE);
                break;
            }
            case 0x24:
            { /* SLA H */
                regH = sla(regH);
                break;
            }
            case 0x25:
            { /* SLA L */
                regL = sla(regL);
                break;
            }
            case 0x26:
            { /* SLA (HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = sla(Z80opsImpl.peek8(work16));
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x27:
            { /* SLA A */
                regA = sla(regA);
                break;
            }
            case 0x28:
            { /* SRA B */
                regB = sra(regB);
                break;
            }
            case 0x29:
            { /* SRA C */
                regC = sra(regC);
                break;
            }
            case 0x2A:
            { /* SRA D */
                regD = sra(regD);
                break;
            }
            case 0x2B:
            { /* SRA E */
                regE = sra(regE);
                break;
            }
            case 0x2C:
            { /* SRA H */
                regH = sra(regH);
                break;
            }
            case 0x2D:
            { /* SRA L */
                regL = sra(regL);
                break;
            }
            case 0x2E:
            { /* SRA (HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = sra(Z80opsImpl.peek8(work16));
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x2F:
            { /* SRA A */
                regA = sra(regA);
                break;
            }
            case 0x30:
            { /* SLL B */
                regB = sll(regB);
                break;
            }
            case 0x31:
            { /* SLL C */
                regC = sll(regC);
                break;
            }
            case 0x32:
            { /* SLL D */
                regD = sll(regD);
                break;
            }
            case 0x33:
            { /* SLL E */
                regE = sll(regE);
                break;
            }
            case 0x34:
            { /* SLL H */
                regH = sll(regH);
                break;
            }
            case 0x35:
            { /* SLL L */
                regL = sll(regL);
                break;
            }
            case 0x36:
            { /* SLL (HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = sll(Z80opsImpl.peek8(work16));
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x37:
            { /* SLL A */
                regA = sll(regA);
                break;
            }
            case 0x38:
            { /* SRL B */
                regB = srl(regB);
                break;
            }
            case 0x39:
            { /* SRL C */
                regC = srl(regC);
                break;
            }
            case 0x3A:
            { /* SRL D */
                regD = srl(regD);
                break;
            }
            case 0x3B:
            { /* SRL E */
                regE = srl(regE);
                break;
            }
            case 0x3C:
            { /* SRL H */
                regH = srl(regH);
                break;
            }
            case 0x3D:
            { /* SRL L */
                regL = srl(regL);
                break;
            }
            case 0x3E:
            { /* SRL (HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = srl(Z80opsImpl.peek8(work16));
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x3F:
            { /* SRL A */
                regA = srl(regA);
                break;
            }
            case 0x40:
            { /* BIT 0,B */
                bit(0x01, regB);
                break;
            }
            case 0x41:
            { /* BIT 0,C */
                bit(0x01, regC);
                break;
            }
            case 0x42:
            { /* BIT 0,D */
                bit(0x01, regD);
                break;
            }
            case 0x43:
            { /* BIT 0,E */
                bit(0x01, regE);
                break;
            }
            case 0x44:
            { /* BIT 0,H */
                bit(0x01, regH);
                break;
            }
            case 0x45:
            { /* BIT 0,L */
                bit(0x01, regL);
                break;
            }
            case 0x46:
            { /* BIT 0,(HL) */
                unsigned int work16 = getRegHL();
                bit(0x01, Z80opsImpl.peek8(work16));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((memptr >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(work16, 1);
                break;
            }
            case 0x47:
            { /* BIT 0,A */
                bit(0x01, regA);
                break;
            }
            case 0x48:
            { /* BIT 1,B */
                bit(0x02, regB);
                break;
            }
            case 0x49:
            { /* BIT 1,C */
                bit(0x02, regC);
                break;
            }
            case 0x4A:
            { /* BIT 1,D */
                bit(0x02, regD);
                break;
            }
            case 0x4B:
            { /* BIT 1,E */
                bit(0x02, regE);
                break;
            }
            case 0x4C:
            { /* BIT 1,H */
                bit(0x02, regH);
                break;
            }
            case 0x4D:
            { /* BIT 1,L */
                bit(0x02, regL);
                break;
            }
            case 0x4E:
            { /* BIT 1,(HL) */
                unsigned int work16 = getRegHL();
                bit(0x02, Z80opsImpl.peek8(work16));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((memptr >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(work16, 1);
                break;
            }
            case 0x4F:
            { /* BIT 1,A */
                bit(0x02, regA);
                break;
            }
            case 0x50:
            { /* BIT 2,B */
                bit(0x04, regB);
                break;
            }
            case 0x51:
            { /* BIT 2,C */
                bit(0x04, regC);
                break;
            }
            case 0x52:
            { /* BIT 2,D */
                bit(0x04, regD);
                break;
            }
            case 0x53:
            { /* BIT 2,E */
                bit(0x04, regE);
                break;
            }
            case 0x54:
            { /* BIT 2,H */
                bit(0x04, regH);
                break;
            }
            case 0x55:
            { /* BIT 2,L */
                bit(0x04, regL);
                break;
            }
            case 0x56:
            { /* BIT 2,(HL) */
                unsigned int work16 = getRegHL();
                bit(0x04, Z80opsImpl.peek8(work16));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((memptr >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(work16, 1);
                break;
            }
            case 0x57:
            { /* BIT 2,A */
                bit(0x04, regA);
                break;
            }
            case 0x58:
            { /* BIT 3,B */
                bit(0x08, regB);
                break;
            }
            case 0x59:
            { /* BIT 3,C */
                bit(0x08, regC);
                break;
            }
            case 0x5A:
            { /* BIT 3,D */
                bit(0x08, regD);
                break;
            }
            case 0x5B:
            { /* BIT 3,E */
                bit(0x08, regE);
                break;
            }
            case 0x5C:
            { /* BIT 3,H */
                bit(0x08, regH);
                break;
            }
            case 0x5D:
            { /* BIT 3,L */
                bit(0x08, regL);
                break;
            }
            case 0x5E:
            { /* BIT 3,(HL) */
                unsigned int work16 = getRegHL();
                bit(0x08, Z80opsImpl.peek8(work16));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((memptr >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(work16, 1);
                break;
            }
            case 0x5F:
            { /* BIT 3,A */
                bit(0x08, regA);
                break;
            }
            case 0x60:
            { /* BIT 4,B */
                bit(0x10, regB);
                break;
            }
            case 0x61:
            { /* BIT 4,C */
                bit(0x10, regC);
                break;
            }
            case 0x62:
            { /* BIT 4,D */
                bit(0x10, regD);
                break;
            }
            case 0x63:
            { /* BIT 4,E */
                bit(0x10, regE);
                break;
            }
            case 0x64:
            { /* BIT 4,H */
                bit(0x10, regH);
                break;
            }
            case 0x65:
            { /* BIT 4,L */
                bit(0x10, regL);
                break;
            }
            case 0x66:
            { /* BIT 4,(HL) */
                unsigned int work16 = getRegHL();
                bit(0x10, Z80opsImpl.peek8(work16));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((memptr >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(work16, 1);
                break;
            }
            case 0x67:
            { /* BIT 4,A */
                bit(0x10, regA);
                break;
            }
            case 0x68:
            { /* BIT 5,B */
                bit(0x20, regB);
                break;
            }
            case 0x69:
            { /* BIT 5,C */
                bit(0x20, regC);
                break;
            }
            case 0x6A:
            { /* BIT 5,D */
                bit(0x20, regD);
                break;
            }
            case 0x6B:
            { /* BIT 5,E */
                bit(0x20, regE);
                break;
            }
            case 0x6C:
            { /* BIT 5,H */
                bit(0x20, regH);
                break;
            }
            case 0x6D:
            { /* BIT 5,L */
                bit(0x20, regL);
                break;
            }
            case 0x6E:
            { /* BIT 5,(HL) */
                unsigned int work16 = getRegHL();
                bit(0x20, Z80opsImpl.peek8(work16));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((memptr >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(work16, 1);
                break;
            }
            case 0x6F:
            { /* BIT 5,A */
                bit(0x20, regA);
                break;
            }
            case 0x70:
            { /* BIT 6,B */
                bit(0x40, regB);
                break;
            }
            case 0x71:
            { /* BIT 6,C */
                bit(0x40, regC);
                break;
            }
            case 0x72:
            { /* BIT 6,D */
                bit(0x40, regD);
                break;
            }
            case 0x73:
            { /* BIT 6,E */
                bit(0x40, regE);
                break;
            }
            case 0x74:
            { /* BIT 6,H */
                bit(0x40, regH);
                break;
            }
            case 0x75:
            { /* BIT 6,L */
                bit(0x40, regL);
                break;
            }
            case 0x76:
            { /* BIT 6,(HL) */
                unsigned int work16 = getRegHL();
                bit(0x40, Z80opsImpl.peek8(work16));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((memptr >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(work16, 1);
                break;
            }
            case 0x77:
            { /* BIT 6,A */
                bit(0x40, regA);
                break;
            }
            case 0x78:
            { /* BIT 7,B */
                bit(0x80, regB);
                break;
            }
            case 0x79:
            { /* BIT 7,C */
                bit(0x80, regC);
                break;
            }
            case 0x7A:
            { /* BIT 7,D */
                bit(0x80, regD);
                break;
            }
            case 0x7B:
            { /* BIT 7,E */
                bit(0x80, regE);
                break;
            }
            case 0x7C:
            { /* BIT 7,H */
                bit(0x80, regH);
                break;
            }
            case 0x7D:
            { /* BIT 7,L */
                bit(0x80, regL);
                break;
            }
            case 0x7E:
            { /* BIT 7,(HL) */
                unsigned int work16 = getRegHL();
                bit(0x80, Z80opsImpl.peek8(work16));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((memptr >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(work16, 1);
                break;
            }
            case 0x7F:
            { /* BIT 7,A */
                bit(0x80, regA);
                break;
            }
            case 0x80:
            { /* RES 0,B */
                regB &= 0xFE;
                break;
            }
            case 0x81:
            { /* RES 0,C */
                regC &= 0xFE;
                break;
            }
            case 0x82:
            { /* RES 0,D */
                regD &= 0xFE;
                break;
            }
            case 0x83:
            { /* RES 0,E */
                regE &= 0xFE;
                break;
            }
            case 0x84:
            { /* RES 0,H */
                regH &= 0xFE;
                break;
            }
            case 0x85:
            { /* RES 0,L */
                regL &= 0xFE;
                break;
            }
            case 0x86:
            { /* RES 0,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) & 0xFE;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x87:
            { /* RES 0,A */
                regA &= 0xFE;
                break;
            }
            case 0x88:
            { /* RES 1,B */
                regB &= 0xFD;
                break;
            }
            case 0x89:
            { /* RES 1,C */
                regC &= 0xFD;
                break;
            }
            case 0x8A:
            { /* RES 1,D */
                regD &= 0xFD;
                break;
            }
            case 0x8B:
            { /* RES 1,E */
                regE &= 0xFD;
                break;
            }
            case 0x8C:
            { /* RES 1,H */
                regH &= 0xFD;
                break;
            }
            case 0x8D:
            { /* RES 1,L */
                regL &= 0xFD;
                break;
            }
            case 0x8E:
            { /* RES 1,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) & 0xFD;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x8F:
            { /* RES 1,A */
                regA &= 0xFD;
                break;
            }
            case 0x90:
            { /* RES 2,B */
                regB &= 0xFB;
                break;
            }
            case 0x91:
            { /* RES 2,C */
                regC &= 0xFB;
                break;
            }
            case 0x92:
            { /* RES 2,D */
                regD &= 0xFB;
                break;
            }
            case 0x93:
            { /* RES 2,E */
                regE &= 0xFB;
                break;
            }
            case 0x94:
            { /* RES 2,H */
                regH &= 0xFB;
                break;
            }
            case 0x95:
            { /* RES 2,L */
                regL &= 0xFB;
                break;
            }
            case 0x96:
            { /* RES 2,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) & 0xFB;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x97:
            { /* RES 2,A */
                regA &= 0xFB;
                break;
            }
            case 0x98:
            { /* RES 3,B */
                regB &= 0xF7;
                break;
            }
            case 0x99:
            { /* RES 3,C */
                regC &= 0xF7;
                break;
            }
            case 0x9A:
            { /* RES 3,D */
                regD &= 0xF7;
                break;
            }
            case 0x9B:
            { /* RES 3,E */
                regE &= 0xF7;
                break;
            }
            case 0x9C:
            { /* RES 3,H */
                regH &= 0xF7;
                break;
            }
            case 0x9D:
            { /* RES 3,L */
                regL &= 0xF7;
                break;
            }
            case 0x9E:
            { /* RES 3,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) & 0xF7;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0x9F:
            { /* RES 3,A */
                regA &= 0xF7;
                break;
            }
            case 0xA0:
            { /* RES 4,B */
                regB &= 0xEF;
                break;
            }
            case 0xA1:
            { /* RES 4,C */
                regC &= 0xEF;
                break;
            }
            case 0xA2:
            { /* RES 4,D */
                regD &= 0xEF;
                break;
            }
            case 0xA3:
            { /* RES 4,E */
                regE &= 0xEF;
                break;
            }
            case 0xA4:
            { /* RES 4,H */
                regH &= 0xEF;
                break;
            }
            case 0xA5:
            { /* RES 4,L */
                regL &= 0xEF;
                break;
            }
            case 0xA6:
            { /* RES 4,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) & 0xEF;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xA7:
            { /* RES 4,A */
                regA &= 0xEF;
                break;
            }
            case 0xA8:
            { /* RES 5,B */
                regB &= 0xDF;
                break;
            }
            case 0xA9:
            { /* RES 5,C */
                regC &= 0xDF;
                break;
            }
            case 0xAA:
            { /* RES 5,D */
                regD &= 0xDF;
                break;
            }
            case 0xAB:
            { /* RES 5,E */
                regE &= 0xDF;
                break;
            }
            case 0xAC:
            { /* RES 5,H */
                regH &= 0xDF;
                break;
            }
            case 0xAD:
            { /* RES 5,L */
                regL &= 0xDF;
                break;
            }
            case 0xAE:
            { /* RES 5,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) & 0xDF;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xAF:
            { /* RES 5,A */
                regA &= 0xDF;
                break;
            }
            case 0xB0:
            { /* RES 6,B */
                regB &= 0xBF;
                break;
            }
            case 0xB1:
            { /* RES 6,C */
                regC &= 0xBF;
                break;
            }
            case 0xB2:
            { /* RES 6,D */
                regD &= 0xBF;
                break;
            }
            case 0xB3:
            { /* RES 6,E */
                regE &= 0xBF;
                break;
            }
            case 0xB4:
            { /* RES 6,H */
                regH &= 0xBF;
                break;
            }
            case 0xB5:
            { /* RES 6,L */
                regL &= 0xBF;
                break;
            }
            case 0xB6:
            { /* RES 6,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) & 0xBF;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xB7:
            { /* RES 6,A */
                regA &= 0xBF;
                break;
            }
            case 0xB8:
            { /* RES 7,B */
                regB &= 0x7F;
                break;
            }
            case 0xB9:
            { /* RES 7,C */
                regC &= 0x7F;
                break;
            }
            case 0xBA:
            { /* RES 7,D */
                regD &= 0x7F;
                break;
            }
            case 0xBB:
            { /* RES 7,E */
                regE &= 0x7F;
                break;
            }
            case 0xBC:
            { /* RES 7,H */
                regH &= 0x7F;
                break;
            }
            case 0xBD:
            { /* RES 7,L */
                regL &= 0x7F;
                break;
            }
            case 0xBE:
            { /* RES 7,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) & 0x7F;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xBF:
            { /* RES 7,A */
                regA &= 0x7F;
                break;
            }
            case 0xC0:
            { /* SET 0,B */
                regB |= 0x01;
                break;
            }
            case 0xC1:
            { /* SET 0,C */
                regC |= 0x01;
                break;
            }
            case 0xC2:
            { /* SET 0,D */
                regD |= 0x01;
                break;
            }
            case 0xC3:
            { /* SET 0,E */
                regE |= 0x01;
                break;
            }
            case 0xC4:
            { /* SET 0,H */
                regH |= 0x01;
                break;
            }
            case 0xC5:
            { /* SET 0,L */
                regL |= 0x01;
                break;
            }
            case 0xC6:
            { /* SET 0,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) | 0x01;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xC7:
            { /* SET 0,A */
                regA |= 0x01;
                break;
            }
            case 0xC8:
            { /* SET 1,B */
                regB |= 0x02;
                break;
            }
            case 0xC9:
            { /* SET 1,C */
                regC |= 0x02;
                break;
            }
            case 0xCA:
            { /* SET 1,D */
                regD |= 0x02;
                break;
            }
            case 0xCB:
            { /* SET 1,E */
                regE |= 0x02;
                break;
            }
            case 0xCC:
            { /* SET 1,H */
                regH |= 0x02;
                break;
            }
            case 0xCD:
            { /* SET 1,L */
                regL |= 0x02;
                break;
            }
            case 0xCE:
            { /* SET 1,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) | 0x02;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xCF:
            { /* SET 1,A */
                regA |= 0x02;
                break;
            }
            case 0xD0:
            { /* SET 2,B */
                regB |= 0x04;
                break;
            }
            case 0xD1:
            { /* SET 2,C */
                regC |= 0x04;
                break;
            }
            case 0xD2:
            { /* SET 2,D */
                regD |= 0x04;
                break;
            }
            case 0xD3:
            { /* SET 2,E */
                regE |= 0x04;
                break;
            }
            case 0xD4:
            { /* SET 2,H */
                regH |= 0x04;
                break;
            }
            case 0xD5:
            { /* SET 2,L */
                regL |= 0x04;
                break;
            }
            case 0xD6:
            { /* SET 2,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) | 0x04;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xD7:
            { /* SET 2,A */
                regA |= 0x04;
                break;
            }
            case 0xD8:
            { /* SET 3,B */
                regB |= 0x08;
                break;
            }
            case 0xD9:
            { /* SET 3,C */
                regC |= 0x08;
                break;
            }
            case 0xDA:
            { /* SET 3,D */
                regD |= 0x08;
                break;
            }
            case 0xDB:
            { /* SET 3,E */
                regE |= 0x08;
                break;
            }
            case 0xDC:
            { /* SET 3,H */
                regH |= 0x08;
                break;
            }
            case 0xDD:
            { /* SET 3,L */
                regL |= 0x08;
                break;
            }
            case 0xDE:
            { /* SET 3,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) | 0x08;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xDF:
            { /* SET 3,A */
                regA |= 0x08;
                break;
            }
            case 0xE0:
            { /* SET 4,B */
                regB |= 0x10;
                break;
            }
            case 0xE1:
            { /* SET 4,C */
                regC |= 0x10;
                break;
            }
            case 0xE2:
            { /* SET 4,D */
                regD |= 0x10;
                break;
            }
            case 0xE3:
            { /* SET 4,E */
                regE |= 0x10;
                break;
            }
            case 0xE4:
            { /* SET 4,H */
                regH |= 0x10;
                break;
            }
            case 0xE5:
            { /* SET 4,L */
                regL |= 0x10;
                break;
            }
            case 0xE6:
            { /* SET 4,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) | 0x10;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xE7:
            { /* SET 4,A */
                regA |= 0x10;
                break;
            }
            case 0xE8:
            { /* SET 5,B */
                regB |= 0x20;
                break;
            }
            case 0xE9:
            { /* SET 5,C */
                regC |= 0x20;
                break;
            }
            case 0xEA:
            { /* SET 5,D */
                regD |= 0x20;
                break;
            }
            case 0xEB:
            { /* SET 5,E */
                regE |= 0x20;
                break;
            }
            case 0xEC:
            { /* SET 5,H */
                regH |= 0x20;
                break;
            }
            case 0xED:
            { /* SET 5,L */
                regL |= 0x20;
                break;
            }
            case 0xEE:
            { /* SET 5,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) | 0x20;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xEF:
            { /* SET 5,A */
                regA |= 0x20;
                break;
            }
            case 0xF0:
            { /* SET 6,B */
                regB |= 0x40;
                break;
            }
            case 0xF1:
            { /* SET 6,C */
                regC |= 0x40;
                break;
            }
            case 0xF2:
            { /* SET 6,D */
                regD |= 0x40;
                break;
            }
            case 0xF3:
            { /* SET 6,E */
                regE |= 0x40;
                break;
            }
            case 0xF4:
            { /* SET 6,H */
                regH |= 0x40;
                break;
            }
            case 0xF5:
            { /* SET 6,L */
                regL |= 0x40;
                break;
            }
            case 0xF6:
            { /* SET 6,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) | 0x40;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xF7:
            { /* SET 6,A */
                regA |= 0x40;
                break;
            }
            case 0xF8:
            { /* SET 7,B */
                regB |= 0x80;
                break;
            }
            case 0xF9:
            { /* SET 7,C */
                regC |= 0x80;
                break;
            }
            case 0xFA:
            { /* SET 7,D */
                regD |= 0x80;
                break;
            }
            case 0xFB:
            { /* SET 7,E */
                regE |= 0x80;
                break;
            }
            case 0xFC:
            { /* SET 7,H */
                regH |= 0x80;
                break;
            }
            case 0xFD:
            { /* SET 7,L */
                regL |= 0x80;
                break;
            }
            case 0xFE:
            { /* SET 7,(HL) */
                unsigned int work16 = getRegHL();
                unsigned int work8 = Z80opsImpl.peek8(work16) | 0x80;
                Z80opsImpl.contendedStates(work16, 1);
                Z80opsImpl.poke8(work16, work8);
                break;
            }
            case 0xFF:
            { /* SET 7,A */
                regA |= 0x80;
                break;
            }
            default:
            {
                //                System.out.println("Error instrucción CB " + Integer.toHexString(opCode));
                break;
            }
        }
    }

    //Subconjunto de instrucciones 0xDD / 0xFD

    /*
     * Hay que tener en cuenta el manejo de secuencias códigos DD/FD que no
     * hacen nada. Según el apartado 3.7 del documento
     * [http://www.myquest.nl/z80undocumented/z80-documented-v0.91.pdf]
     * secuencias de códigos como FD DD 00 21 00 10 NOP NOP NOP LD HL,1000h
     * activan IY con el primer FD, IX con el segundo DD y vuelven al
     * registro HL con el código NOP. Es decir, si detrás del código DD/FD no
     * viene una instrucción que maneje el registro HL, el código DD/FD
     * "se olvida" y hay que procesar la instrucción como si nunca se
     * hubiera visto el prefijo (salvo por los 4 t-estados que ha costado).
     * Naturalmente, en una serie repetida de DDFD no hay que comprobar las
     * interrupciones entre cada prefijo.
     */
    uint16_t decodeDDFD(uint16_t regIXY) {

        regR++;
        opCode = Z80opsImpl.fetchOpcode(regPC);
        regPC++;

        switch (opCode) {
            case 0x09:
            { /* ADD IX,BC */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                regIXY = add16(regIXY, getRegBC());
                break;
            }
            case 0x19:
            { /* ADD IX,DE */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                regIXY = add16(regIXY, getRegDE());
                break;
            }
            case 0x21:
            { /* LD IX,nn */
                regIXY = Z80opsImpl.peek16(regPC);
                regPC = regPC + 2;
                break;
            }
            case 0x22:
            { /* LD (nn),IX */
                memptr = Z80opsImpl.peek16(regPC);
                Z80opsImpl.poke16(memptr++, regIXY);
                regPC = regPC + 2;
                break;
            }
            case 0x23:
            { /* INC IX */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                regIXY = (regIXY + 1) & 0xffff;
                break;
            }
            case 0x24:
            { /* INC IXh */
                regIXY = (inc8(regIXY >> 8) << 8) | (regIXY & 0xff);
                break;
            }
            case 0x25:
            { /* DEC IXh */
                regIXY = (dec8(regIXY >> 8) << 8) | (regIXY & 0xff);
                break;
            }
            case 0x26:
            { /* LD IXh,n */
                regIXY = (Z80opsImpl.peek8(regPC) << 8) | (regIXY & 0xff);
                regPC++;
                break;
            }
            case 0x29:
            { /* ADD IX,IX */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                regIXY = add16(regIXY, regIXY);
                break;
            }
            case 0x2A:
            { /* LD IX,(nn) */
                memptr = Z80opsImpl.peek16(regPC);
                regIXY = Z80opsImpl.peek16(memptr++);
                regPC = regPC + 2;
                break;
            }
            case 0x2B:
            { /* DEC IX */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                regIXY = (regIXY - 1) & 0xffff;
                break;
            }
            case 0x2C:
            { /* INC IXl */
                regIXY = (regIXY & 0xff00) | inc8(regIXY & 0xff);
                break;
            }
            case 0x2D:
            { /* DEC IXl */
                regIXY = (regIXY & 0xff00) | dec8(regIXY & 0xff);
                break;
            }
            case 0x2E:
            { /* LD IXl,n */
                regIXY = (regIXY & 0xff00) | Z80opsImpl.peek8(regPC);
                regPC++;
                break;
            }
            case 0x34:
            { /* INC (IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                uint8_t work8 = Z80opsImpl.peek8(memptr);
                Z80opsImpl.contendedStates(memptr, 1);
                Z80opsImpl.poke8(memptr, inc8(work8));
                regPC++;
                break;
            }
            case 0x35:
            { /* DEC (IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                uint8_t work8 = Z80opsImpl.peek8(memptr);
                Z80opsImpl.contendedStates(memptr, 1);
                Z80opsImpl.poke8(memptr, dec8(work8));
                regPC++;
                break;
            }
            case 0x36:
            { /* LD (IX+d),n */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                regPC++;
                unsigned int work8 = Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 2);
                regPC++;
                Z80opsImpl.poke8(memptr, work8);
                break;
            }
            case 0x39:
            { /* ADD IX,SP */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                regIXY = add16(regIXY, regSP);
                break;
            }
            case 0x44:
            { /* LD B,IXh */
                regB = regIXY >> 8;
                break;
            }
            case 0x45:
            { /* LD B,IXl */
                regB = regIXY & 0xff;
                break;
            }
            case 0x46:
            { /* LD B,(IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                regB = Z80opsImpl.peek8(memptr);
                regPC++;
                break;
            }
            case 0x4C:
            { /* LD C,IXh */
                regC = regIXY >> 8;
                break;
            }
            case 0x4D:
            { /* LD C,IXl */
                regC = regIXY & 0xff;
                break;
            }
            case 0x4E:
            { /* LD C,(IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                regC = Z80opsImpl.peek8(memptr);
                regPC++;
                break;
            }
            case 0x54:
            { /* LD D,IXh */
                regD = regIXY >> 8;
                break;
            }
            case 0x55:
            { /* LD D,IXl */
                regD = regIXY & 0xff;
                break;
            }
            case 0x56:
            { /* LD D,(IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                regD = Z80opsImpl.peek8(memptr);
                regPC++;
                break;
            }
            case 0x5C:
            { /* LD E,IXh */
                regE = regIXY >> 8;
                break;
            }
            case 0x5D:
            { /* LD E,IXl */
                regE = regIXY & 0xff;
                break;
            }
            case 0x5E:
            { /* LD E,(IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                regE = Z80opsImpl.peek8(memptr);
                regPC++;
                break;
            }
            case 0x60:
            { /* LD IXh,B */
                regIXY = (regIXY & 0x00ff) | (regB << 8);
                break;
            }
            case 0x61:
            { /* LD IXh,C */
                regIXY = (regIXY & 0x00ff) | (regC << 8);
                break;
            }
            case 0x62:
            { /* LD IXh,D */
                regIXY = (regIXY & 0x00ff) | (regD << 8);
                break;
            }
            case 0x63:
            { /* LD IXh,E */
                regIXY = (regIXY & 0x00ff) | (regE << 8);
                break;
            }
            case 0x64:
            { /* LD IXh,IXh */
                break;
            }
            case 0x65:
            { /* LD IXh,IXl */
                regIXY = (regIXY & 0x00ff) | ((regIXY & 0xff) << 8);
                break;
            }
            case 0x66:
            { /* LD H,(IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                regH = Z80opsImpl.peek8(memptr);
                regPC++;
                break;
            }
            case 0x67:
            { /* LD IXh,A */
                regIXY = (regIXY & 0x00ff) | (regA << 8);
                break;
            }
            case 0x68:
            { /* LD IXl,B */
                regIXY = (regIXY & 0xff00) | regB;
                break;
            }
            case 0x69:
            { /* LD IXl,C */
                regIXY = (regIXY & 0xff00) | regC;
                break;
            }
            case 0x6A:
            { /* LD IXl,D */
                regIXY = (regIXY & 0xff00) | regD;
                break;
            }
            case 0x6B:
            { /* LD IXl,E */
                regIXY = (regIXY & 0xff00) | regE;
                break;
            }
            case 0x6C:
            { /* LD IXl,IXh */
                regIXY = (regIXY & 0xff00) | (regIXY >> 8);
                break;
            }
            case 0x6D:
            { /* LD IXl,IXl */
                break;
            }
            case 0x6E:
            { /* LD L,(IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                regL = Z80opsImpl.peek8(memptr);
                regPC++;
                break;
            }
            case 0x6F:
            { /* LD IXl,A */
                regIXY = (regIXY & 0xff00) | regA;
                break;
            }
            case 0x70:
            { /* LD (IX+d),B */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                Z80opsImpl.poke8(memptr, regB);
                regPC++;
                break;
            }
            case 0x71:
            { /* LD (IX+d),C */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                Z80opsImpl.poke8(memptr, regC);
                regPC++;
                break;
            }
            case 0x72:
            { /* LD (IX+d),D */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                Z80opsImpl.poke8(memptr, regD);
                regPC++;
                break;
            }
            case 0x73:
            { /* LD (IX+d),E */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                Z80opsImpl.poke8(memptr, regE);
                regPC++;
                break;
            }
            case 0x74:
            { /* LD (IX+d),H */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                Z80opsImpl.poke8(memptr, regH);
                regPC++;
                break;
            }
            case 0x75:
            { /* LD (IX+d),L */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                Z80opsImpl.poke8(memptr, regL);
                regPC++;
                break;
            }
            case 0x77:
            { /* LD (IX+d),A */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                Z80opsImpl.poke8(memptr, regA);
                regPC++;
                break;
            }
            case 0x7C:
            { /* LD A,IXh */
                regA = regIXY >> 8;
                break;
            }
            case 0x7D:
            { /* LD A,IXl */
                regA = regIXY & 0xff;
                break;
            }
            case 0x7E:
            { /* LD A,(IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                regA = Z80opsImpl.peek8(memptr);
                regPC++;
                break;
            }
            case 0x84:
            { /* ADD A,IXh */
                carryFlag = false;
                adc(regIXY >> 8);
                break;
            }
            case 0x85:
            { /* ADD A,IXl */
                carryFlag = false;
                adc(regIXY & 0xff);
                break;
            }
            case 0x86:
            { /* ADD A,(IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                carryFlag = false;
                adc(Z80opsImpl.peek8(memptr));
                regPC++;
                break;
            }
            case 0x8C:
            { /* ADC A,IXh */
                adc(regIXY >> 8);
                break;
            }
            case 0x8D:
            { /* ADC A,IXl */
                adc(regIXY & 0xff);
                break;
            }
            case 0x8E:
            { /* ADC A,(IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                adc(Z80opsImpl.peek8(memptr));
                regPC++;
                break;
            }
            case 0x94:
            { /* SUB IXh */
                carryFlag = false;
                sbc(regIXY >> 8);
                break;
            }
            case 0x95:
            { /* SUB IXl */
                carryFlag = false;
                sbc(regIXY & 0xff);
                break;
            }
            case 0x96:
            { /* SUB (IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                carryFlag = false;
                sbc(Z80opsImpl.peek8(memptr));
                regPC++;
                break;
            }
            case 0x9C:
            { /* SBC A,IXh */
                sbc(regIXY >> 8);
                break;
            }
            case 0x9D:
            { /* SBC A,IXl */
                sbc(regIXY & 0xff);
                break;
            }
            case 0x9E:
            { /* SBC A,(IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                sbc(Z80opsImpl.peek8(memptr));
                regPC++;
                break;
            }
            case 0xA4:
            { /* AND IXh */
                and_(regIXY >> 8);
                break;
            }
            case 0xA5:
            { /* AND IXl */
                and_(regIXY & 0xff);
                break;
            }
            case 0xA6:
            { /* AND (IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                and_(Z80opsImpl.peek8(memptr));
                regPC++;
                break;
            }
            case 0xAC:
            { /* XOR IXh */
                xor_(regIXY >> 8);
                break;
            }
            case 0xAD:
            { /* XOR IXl */
                xor_(regIXY & 0xff);
                break;
            }
            case 0xAE:
            { /* XOR (IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                xor_(Z80opsImpl.peek8(memptr));
                regPC++;
                break;
            }
            case 0xB4:
            { /* OR IXh */
                or_(regIXY >> 8);
                break;
            }
            case 0xB5:
            { /* OR IXl */
                or_(regIXY & 0xff);
                break;
            }
            case 0xB6:
            { /* OR (IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                or_(Z80opsImpl.peek8(memptr));
                regPC++;
                break;
            }
            case 0xBC:
            { /* CP IXh */
                cp(regIXY >> 8);
                break;
            }
            case 0xBD:
            { /* CP IXl */
                cp(regIXY & 0xff);
                break;
            }
            case 0xBE:
            { /* CP (IX+d) */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 5);
                cp(Z80opsImpl.peek8(memptr));
                regPC++;
                break;
            }
            case 0xCB:
            { /* Subconjunto de instrucciones */
                memptr = regIXY + (uint8_t) Z80opsImpl.peek8(regPC);
                regPC++;
                opCode = Z80opsImpl.peek8(regPC);
                Z80opsImpl.contendedStates(regPC, 2);
                regPC++;
                if (opCode < 0x80) {
                    decodeDDFDCBto7F(opCode, memptr);
                } else {
                    decodeDDFDCBtoFF(opCode, memptr);
                }
                break;
            }
            case 0xE1:
            { /* POP IX */
                regIXY = pop();
                break;
            }
            case 0xE3:
            { /* EX (SP),IX */
                // Instrucción de ejecución sutil como pocas... atento al dato.
                uint16_t work16 = regIXY;
                regIXY = Z80opsImpl.peek16(regSP);
                Z80opsImpl.contendedStates(regSP + 1, 1);
                Z80opsImpl.poke8(regSP + 1, work16 >> 8);
                Z80opsImpl.poke8(regSP, work16);
                Z80opsImpl.contendedStates(regSP, 2);
                memptr = regIXY;
                break;
            }
            case 0xE5:
            { /* PUSH IX */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                push(regIXY);
                break;
            }
            case 0xE9:
            { /* JP (IX) */
                regPC = regIXY;
                break;
            }
            case 0xF9:
            { /* LD SP,IX */
                Z80opsImpl.contendedStates(getPairIR(), 2);
                regSP = regIXY;
                break;
            }
            default:
            {
                // Detrás de un DD/FD o varios en secuencia venía un código
                // que no correspondía con una instrucción que involucra a 
                // IX o IY. Se trata como si fuera un código normal.
                // Sin esto, además de emular mal, falla el test
                // ld <bcdexya>,<bcdexya> de ZEXALL.

                //                System.out.println("Error instrucción DD/FD" + Integer.toHexString(opCode));

                if (breakpointAt[regPC]) {
                    Z80opsImpl.breakpoint();
                }

                decodeOpcode(opCode);
                break;
            }
        }
        return regIXY;
    }

    // Subconjunto de instrucciones 0xDDCB desde el código 0x00 hasta el 0x7F

    void decodeDDFDCBto7F(uint8_t opCode, uint16_t address) {

        switch (opCode) {
            case 0x00:
            { /* RLC (IX+d),B */
                regB = rlc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x01:
            { /* RLC (IX+d),C */
                regC = rlc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x02:
            { /* RLC (IX+d),D */
                regD = rlc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x03:
            { /* RLC (IX+d),E */
                regE = rlc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x04:
            { /* RLC (IX+d),H */
                regH = rlc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x05:
            { /* RLC (IX+d),L */
                regL = rlc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x06:
            { /* RLC (IX+d) */
                unsigned int work8 = rlc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x07:
            { /* RLC (IX+d),A */
                regA = rlc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x08:
            { /* RRC (IX+d),B */
                regB = rrc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x09:
            { /* RRC (IX+d),C */
                regC = rrc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x0A:
            { /* RRC (IX+d),D */
                regD = rrc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x0B:
            { /* RRC (IX+d),E */
                regE = rrc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x0C:
            { /* RRC (IX+d),H */
                regH = rrc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x0D:
            { /* RRC (IX+d),L */
                regL = rrc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x0E:
            { /* RRC (IX+d) */
                unsigned int work8 = rrc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x0F:
            { /* RRC (IX+d),A */
                regA = rrc(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x10:
            { /* RL (IX+d),B */
                regB = rl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x11:
            { /* RL (IX+d),C */
                regC = rl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x12:
            { /* RL (IX+d),D */
                regD = rl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x13:
            { /* RL (IX+d),E */
                regE = rl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x14:
            { /* RL (IX+d),H */
                regH = rl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x15:
            { /* RL (IX+d),L */
                regL = rl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x16:
            { /* RL (IX+d) */
                unsigned int work8 = rl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x17:
            { /* RL (IX+d),A */
                regA = rl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x18:
            { /* RR (IX+d),B */
                regB = rr(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x19:
            { /* RR (IX+d),C */
                regC = rr(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x1A:
            { /* RR (IX+d),D */
                regD = rr(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x1B:
            { /* RR (IX+d),E */
                regE = rr(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x1C:
            { /* RR (IX+d),H */
                regH = rr(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x1D:
            { /* RR (IX+d),L */
                regL = rr(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x1E:
            { /* RR (IX+d) */
                unsigned int work8 = rr(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x1F:
            { /* RR (IX+d),A */
                regA = rr(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x20:
            { /* SLA (IX+d),B */
                regB = sla(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x21:
            { /* SLA (IX+d),C */
                regC = sla(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x22:
            { /* SLA (IX+d),D */
                regD = sla(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x23:
            { /* SLA (IX+d),E */
                regE = sla(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x24:
            { /* SLA (IX+d),H */
                regH = sla(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x25:
            { /* SLA (IX+d),L */
                regL = sla(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x26:
            { /* SLA (IX+d) */
                unsigned int work8 = sla(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x27:
            { /* SLA (IX+d),A */
                regA = sla(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x28:
            { /* SRA (IX+d),B */
                regB = sra(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x29:
            { /* SRA (IX+d),C */
                regC = sra(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x2A:
            { /* SRA (IX+d),D */
                regD = sra(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x2B:
            { /* SRA (IX+d),E */
                regE = sra(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x2C:
            { /* SRA (IX+d),H */
                regH = sra(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x2D:
            { /* SRA (IX+d),L */
                regL = sra(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x2E:
            { /* SRA (IX+d) */
                unsigned int work8 = sra(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x2F:
            { /* SRA (IX+d),A */
                regA = sra(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x30:
            { /* SLL (IX+d),B */
                regB = sll(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x31:
            { /* SLL (IX+d),C */
                regC = sll(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x32:
            { /* SLL (IX+d),D */
                regD = sll(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x33:
            { /* SLL (IX+d),E */
                regE = sll(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x34:
            { /* SLL (IX+d),H */
                regH = sll(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x35:
            { /* SLL (IX+d),L */
                regL = sll(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x36:
            { /* SLL (IX+d) */
                unsigned int work8 = sll(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x37:
            { /* SLL (IX+d),A */
                regA = sll(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x38:
            { /* SRL (IX+d),B */
                regB = srl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x39:
            { /* SRL (IX+d),C */
                regC = srl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x3A:
            { /* SRL (IX+d),D */
                regD = srl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x3B:
            { /* SRL (IX+d),E */
                regE = srl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x3C:
            { /* SRL (IX+d),H */
                regH = srl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x3D:
            { /* SRL (IX+d),L */
                regL = srl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x3E:
            { /* SRL (IX+d) */
                unsigned int work8 = srl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x3F:
            { /* SRL (IX+d),A */
                regA = srl(Z80opsImpl.peek8(address));
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x40:
            case 0x41:
            case 0x42:
            case 0x43:
            case 0x44:
            case 0x45:
            case 0x46:
            case 0x47:
            { /* BIT 0,(IX+d) */
                bit(0x01, Z80opsImpl.peek8(address));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((address >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(address, 1);
                break;
            }
            case 0x48:
            case 0x49:
            case 0x4A:
            case 0x4B:
            case 0x4C:
            case 0x4D:
            case 0x4E:
            case 0x4F:
            { /* BIT 1,(IX+d) */
                bit(0x02, Z80opsImpl.peek8(address));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((address >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(address, 1);
                break;
            }
            case 0x50:
            case 0x51:
            case 0x52:
            case 0x53:
            case 0x54:
            case 0x55:
            case 0x56:
            case 0x57:
            { /* BIT 2,(IX+d) */
                bit(0x04, Z80opsImpl.peek8(address));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((address >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(address, 1);
                break;
            }
            case 0x58:
            case 0x59:
            case 0x5A:
            case 0x5B:
            case 0x5C:
            case 0x5D:
            case 0x5E:
            case 0x5F:
            { /* BIT 3,(IX+d) */
                bit(0x08, Z80opsImpl.peek8(address));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((address >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(address, 1);
                break;
            }
            case 0x60:
            case 0x61:
            case 0x62:
            case 0x63:
            case 0x64:
            case 0x65:
            case 0x66:
            case 0x67:
            { /* BIT 4,(IX+d) */
                bit(0x10, Z80opsImpl.peek8(address));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((address >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(address, 1);
                break;
            }
            case 0x68:
            case 0x69:
            case 0x6A:
            case 0x6B:
            case 0x6C:
            case 0x6D:
            case 0x6E:
            case 0x6F:
            { /* BIT 5,(IX+d) */
                bit(0x20, Z80opsImpl.peek8(address));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((address >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(address, 1);
                break;
            }
            case 0x70:
            case 0x71:
            case 0x72:
            case 0x73:
            case 0x74:
            case 0x75:
            case 0x76:
            case 0x77:
            { /* BIT 6,(IX+d) */
                bit(0x40, Z80opsImpl.peek8(address));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((address >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(address, 1);
                break;
            }
            case 0x78:
            case 0x79:
            case 0x7A:
            case 0x7B:
            case 0x7C:
            case 0x7D:
            case 0x7E:
            case 0x7F:
            { /* BIT 7,(IX+d) */
                bit(0x80, Z80opsImpl.peek8(address));
                sz5h3pnFlags = (sz5h3pnFlags & FLAG_SZHP_MASK)
                        | ((address >> 8) & FLAG_53_MASK);
                Z80opsImpl.contendedStates(address, 1);
                break;
            }
        }
    }

    // Subconjunto de instrucciones 0xDDCB desde el código 0x80 hasta el 0xFF

    void decodeDDFDCBtoFF(uint8_t opCode, uint16_t address) {

        switch (opCode) {
            case 0x80:
            { /* RES 0,(IX+d),B */
                regB = Z80opsImpl.peek8(address) & 0xFE;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x81:
            { /* RES 0,(IX+d),C */
                regC = Z80opsImpl.peek8(address) & 0xFE;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x82:
            { /* RES 0,(IX+d),D */
                regD = Z80opsImpl.peek8(address) & 0xFE;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x83:
            { /* RES 0,(IX+d),E */
                regE = Z80opsImpl.peek8(address) & 0xFE;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x84:
            { /* RES 0,(IX+d),H */
                regH = Z80opsImpl.peek8(address) & 0xFE;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x85:
            { /* RES 0,(IX+d),L */
                regL = Z80opsImpl.peek8(address) & 0xFE;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x86:
            { /* RES 0,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) & 0xFE;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x87:
            { /* RES 0,(IX+d),A */
                regA = Z80opsImpl.peek8(address) & 0xFE;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x88:
            { /* RES 1,(IX+d),B */
                regB = Z80opsImpl.peek8(address) & 0xFD;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x89:
            { /* RES 1,(IX+d),C */
                regC = Z80opsImpl.peek8(address) & 0xFD;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x8A:
            { /* RES 1,(IX+d),D */
                regD = Z80opsImpl.peek8(address) & 0xFD;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x8B:
            { /* RES 1,(IX+d),E */
                regE = Z80opsImpl.peek8(address) & 0xFD;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x8C:
            { /* RES 1,(IX+d),H */
                regH = Z80opsImpl.peek8(address) & 0xFD;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x8D:
            { /* RES 1,(IX+d),L */
                regL = Z80opsImpl.peek8(address) & 0xFD;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x8E:
            { /* RES 1,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) & 0xFD;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x8F:
            { /* RES 1,(IX+d),A */
                regA = Z80opsImpl.peek8(address) & 0xFD;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x90:
            { /* RES 2,(IX+d),B */
                regB = Z80opsImpl.peek8(address) & 0xFB;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x91:
            { /* RES 2,(IX+d),C */
                regC = Z80opsImpl.peek8(address) & 0xFB;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x92:
            { /* RES 2,(IX+d),D */
                regD = Z80opsImpl.peek8(address) & 0xFB;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x93:
            { /* RES 2,(IX+d),E */
                regE = Z80opsImpl.peek8(address) & 0xFB;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x94:
            { /* RES 2,(IX+d),H */
                regH = Z80opsImpl.peek8(address) & 0xFB;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x95:
            { /* RES 2,(IX+d),L */
                regL = Z80opsImpl.peek8(address) & 0xFB;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x96:
            { /* RES 2,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) & 0xFB;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x97:
            { /* RES 2,(IX+d),A */
                regA = Z80opsImpl.peek8(address) & 0xFB;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0x98:
            { /* RES 3,(IX+d),B */
                regB = Z80opsImpl.peek8(address) & 0xF7;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0x99:
            { /* RES 3,(IX+d),C */
                regC = Z80opsImpl.peek8(address) & 0xF7;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0x9A:
            { /* RES 3,(IX+d),D */
                regD = Z80opsImpl.peek8(address) & 0xF7;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0x9B:
            { /* RES 3,(IX+d),E */
                regE = Z80opsImpl.peek8(address) & 0xF7;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0x9C:
            { /* RES 3,(IX+d),H */
                regH = Z80opsImpl.peek8(address) & 0xF7;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0x9D:
            { /* RES 3,(IX+d),L */
                regL = Z80opsImpl.peek8(address) & 0xF7;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0x9E:
            { /* RES 3,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) & 0xF7;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0x9F:
            { /* RES 3,(IX+d),A */
                regA = Z80opsImpl.peek8(address) & 0xF7;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xA0:
            { /* RES 4,(IX+d),B */
                regB = Z80opsImpl.peek8(address) & 0xEF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xA1:
            { /* RES 4,(IX+d),C */
                regC = Z80opsImpl.peek8(address) & 0xEF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xA2:
            { /* RES 4,(IX+d),D */
                regD = Z80opsImpl.peek8(address) & 0xEF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xA3:
            { /* RES 4,(IX+d),E */
                regE = Z80opsImpl.peek8(address) & 0xEF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xA4:
            { /* RES 4,(IX+d),H */
                regH = Z80opsImpl.peek8(address) & 0xEF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xA5:
            { /* RES 4,(IX+d),L */
                regL = Z80opsImpl.peek8(address) & 0xEF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xA6:
            { /* RES 4,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) & 0xEF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xA7:
            { /* RES 4,(IX+d),A */
                regA = Z80opsImpl.peek8(address) & 0xEF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xA8:
            { /* RES 5,(IX+d),B */
                regB = Z80opsImpl.peek8(address) & 0xDF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xA9:
            { /* RES 5,(IX+d),C */
                regC = Z80opsImpl.peek8(address) & 0xDF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xAA:
            { /* RES 5,(IX+d),D */
                regD = Z80opsImpl.peek8(address) & 0xDF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xAB:
            { /* RES 5,(IX+d),E */
                regE = Z80opsImpl.peek8(address) & 0xDF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xAC:
            { /* RES 5,(IX+d),H */
                regH = Z80opsImpl.peek8(address) & 0xDF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xAD:
            { /* RES 5,(IX+d),L */
                regL = Z80opsImpl.peek8(address) & 0xDF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xAE:
            { /* RES 5,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) & 0xDF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xAF:
            { /* RES 5,(IX+d),A */
                regA = Z80opsImpl.peek8(address) & 0xDF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xB0:
            { /* RES 6,(IX+d),B */
                regB = Z80opsImpl.peek8(address) & 0xBF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xB1:
            { /* RES 6,(IX+d),C */
                regC = Z80opsImpl.peek8(address) & 0xBF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xB2:
            { /* RES 6,(IX+d),D */
                regD = Z80opsImpl.peek8(address) & 0xBF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xB3:
            { /* RES 6,(IX+d),E */
                regE = Z80opsImpl.peek8(address) & 0xBF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xB4:
            { /* RES 6,(IX+d),H */
                regH = Z80opsImpl.peek8(address) & 0xBF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xB5:
            { /* RES 6,(IX+d),L */
                regL = Z80opsImpl.peek8(address) & 0xBF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xB6:
            { /* RES 6,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) & 0xBF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xB7:
            { /* RES 6,(IX+d),A */
                regA = Z80opsImpl.peek8(address) & 0xBF;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xB8:
            { /* RES 7,(IX+d),B */
                regB = Z80opsImpl.peek8(address) & 0x7F;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xB9:
            { /* RES 7,(IX+d),C */
                regC = Z80opsImpl.peek8(address) & 0x7F;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xBA:
            { /* RES 7,(IX+d),D */
                regD = Z80opsImpl.peek8(address) & 0x7F;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xBB:
            { /* RES 7,(IX+d),E */
                regE = Z80opsImpl.peek8(address) & 0x7F;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xBC:
            { /* RES 7,(IX+d),H */
                regH = Z80opsImpl.peek8(address) & 0x7F;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xBD:
            { /* RES 7,(IX+d),L */
                regL = Z80opsImpl.peek8(address) & 0x7F;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xBE:
            { /* RES 7,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) & 0x7F;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xBF:
            { /* RES 7,(IX+d),A */
                regA = Z80opsImpl.peek8(address) & 0x7F;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xC0:
            { /* SET 0,(IX+d),B */
                regB = Z80opsImpl.peek8(address) | 0x01;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xC1:
            { /* SET 0,(IX+d),C */
                regC = Z80opsImpl.peek8(address) | 0x01;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xC2:
            { /* SET 0,(IX+d),D */
                regD = Z80opsImpl.peek8(address) | 0x01;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xC3:
            { /* SET 0,(IX+d),E */
                regE = Z80opsImpl.peek8(address) | 0x01;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xC4:
            { /* SET 0,(IX+d),H */
                regH = Z80opsImpl.peek8(address) | 0x01;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xC5:
            { /* SET 0,(IX+d),L */
                regL = Z80opsImpl.peek8(address) | 0x01;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xC6:
            { /* SET 0,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) | 0x01;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xC7:
            { /* SET 0,(IX+d),A */
                regA = Z80opsImpl.peek8(address) | 0x01;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xC8:
            { /* SET 1,(IX+d),B */
                regB = Z80opsImpl.peek8(address) | 0x02;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xC9:
            { /* SET 1,(IX+d),C */
                regC = Z80opsImpl.peek8(address) | 0x02;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xCA:
            { /* SET 1,(IX+d),D */
                regD = Z80opsImpl.peek8(address) | 0x02;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xCB:
            { /* SET 1,(IX+d),E */
                regE = Z80opsImpl.peek8(address) | 0x02;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xCC:
            { /* SET 1,(IX+d),H */
                regH = Z80opsImpl.peek8(address) | 0x02;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xCD:
            { /* SET 1,(IX+d),L */
                regL = Z80opsImpl.peek8(address) | 0x02;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xCE:
            { /* SET 1,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) | 0x02;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xCF:
            { /* SET 1,(IX+d),A */
                regA = Z80opsImpl.peek8(address) | 0x02;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xD0:
            { /* SET 2,(IX+d),B */
                regB = Z80opsImpl.peek8(address) | 0x04;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xD1:
            { /* SET 2,(IX+d),C */
                regC = Z80opsImpl.peek8(address) | 0x04;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xD2:
            { /* SET 2,(IX+d),D */
                regD = Z80opsImpl.peek8(address) | 0x04;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xD3:
            { /* SET 2,(IX+d),E */
                regE = Z80opsImpl.peek8(address) | 0x04;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xD4:
            { /* SET 2,(IX+d),H */
                regH = Z80opsImpl.peek8(address) | 0x04;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xD5:
            { /* SET 2,(IX+d),L */
                regL = Z80opsImpl.peek8(address) | 0x04;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xD6:
            { /* SET 2,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) | 0x04;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xD7:
            { /* SET 2,(IX+d),A */
                regA = Z80opsImpl.peek8(address) | 0x04;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xD8:
            { /* SET 3,(IX+d),B */
                regB = Z80opsImpl.peek8(address) | 0x08;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xD9:
            { /* SET 3,(IX+d),C */
                regC = Z80opsImpl.peek8(address) | 0x08;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xDA:
            { /* SET 3,(IX+d),D */
                regD = Z80opsImpl.peek8(address) | 0x08;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xDB:
            { /* SET 3,(IX+d),E */
                regE = Z80opsImpl.peek8(address) | 0x08;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xDC:
            { /* SET 3,(IX+d),H */
                regH = Z80opsImpl.peek8(address) | 0x08;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xDD:
            { /* SET 3,(IX+d),L */
                regL = Z80opsImpl.peek8(address) | 0x08;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xDE:
            { /* SET 3,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) | 0x08;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xDF:
            { /* SET 3,(IX+d),A */
                regA = Z80opsImpl.peek8(address) | 0x08;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xE0:
            { /* SET 4,(IX+d),B */
                regB = Z80opsImpl.peek8(address) | 0x10;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xE1:
            { /* SET 4,(IX+d),C */
                regC = Z80opsImpl.peek8(address) | 0x10;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xE2:
            { /* SET 4,(IX+d),D */
                regD = Z80opsImpl.peek8(address) | 0x10;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xE3:
            { /* SET 4,(IX+d),E */
                regE = Z80opsImpl.peek8(address) | 0x10;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xE4:
            { /* SET 4,(IX+d),H */
                regH = Z80opsImpl.peek8(address) | 0x10;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xE5:
            { /* SET 4,(IX+d),L */
                regL = Z80opsImpl.peek8(address) | 0x10;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xE6:
            { /* SET 4,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) | 0x10;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xE7:
            { /* SET 4,(IX+d),A */
                regA = Z80opsImpl.peek8(address) | 0x10;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xE8:
            { /* SET 5,(IX+d),B */
                regB = Z80opsImpl.peek8(address) | 0x20;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xE9:
            { /* SET 5,(IX+d),C */
                regC = Z80opsImpl.peek8(address) | 0x20;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xEA:
            { /* SET 5,(IX+d),D */
                regD = Z80opsImpl.peek8(address) | 0x20;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xEB:
            { /* SET 5,(IX+d),E */
                regE = Z80opsImpl.peek8(address) | 0x20;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xEC:
            { /* SET 5,(IX+d),H */
                regH = Z80opsImpl.peek8(address) | 0x20;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xED:
            { /* SET 5,(IX+d),L */
                regL = Z80opsImpl.peek8(address) | 0x20;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xEE:
            { /* SET 5,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) | 0x20;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xEF:
            { /* SET 5,(IX+d),A */
                regA = Z80opsImpl.peek8(address) | 0x20;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xF0:
            { /* SET 6,(IX+d),B */
                regB = Z80opsImpl.peek8(address) | 0x40;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xF1:
            { /* SET 6,(IX+d),C */
                regC = Z80opsImpl.peek8(address) | 0x40;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xF2:
            { /* SET 6,(IX+d),D */
                regD = Z80opsImpl.peek8(address) | 0x40;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xF3:
            { /* SET 6,(IX+d),E */
                regE = Z80opsImpl.peek8(address) | 0x40;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xF4:
            { /* SET 6,(IX+d),H */
                regH = Z80opsImpl.peek8(address) | 0x40;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xF5:
            { /* SET 6,(IX+d),L */
                regL = Z80opsImpl.peek8(address) | 0x40;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xF6:
            { /* SET 6,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) | 0x40;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xF7:
            { /* SET 6,(IX+d),A */
                regA = Z80opsImpl.peek8(address) | 0x40;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
            case 0xF8:
            { /* SET 7,(IX+d),B */
                regB = Z80opsImpl.peek8(address) | 0x80;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regB);
                break;
            }
            case 0xF9:
            { /* SET 7,(IX+d),C */
                regC = Z80opsImpl.peek8(address) | 0x80;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regC);
                break;
            }
            case 0xFA:
            { /* SET 7,(IX+d),D */
                regD = Z80opsImpl.peek8(address) | 0x80;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regD);
                break;
            }
            case 0xFB:
            { /* SET 7,(IX+d),E */
                regE = Z80opsImpl.peek8(address) | 0x80;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regE);
                break;
            }
            case 0xFC:
            { /* SET 7,(IX+d),H */
                regH = Z80opsImpl.peek8(address) | 0x80;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regH);
                break;
            }
            case 0xFD:
            { /* SET 7,(IX+d),L */
                regL = Z80opsImpl.peek8(address) | 0x80;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regL);
                break;
            }
            case 0xFE:
            { /* SET 7,(IX+d) */
                unsigned int work8 = Z80opsImpl.peek8(address) | 0x80;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, work8);
                break;
            }
            case 0xFF:
            { /* SET 7,(IX+d),A */
                regA = Z80opsImpl.peek8(address) | 0x80;
                Z80opsImpl.contendedStates(address, 1);
                Z80opsImpl.poke8(address, regA);
                break;
            }
        }
    }

    //Subconjunto de instrucciones 0xED

    void decodeED() {

        regR++;
        opCode = Z80opsImpl.fetchOpcode(regPC);
        regPC++;

        switch (opCode) {
            case 0x40:
            { /* IN B,(C) */
                memptr = getRegBC();
                regB = Z80opsImpl.inPort(memptr++);
                sz5h3pnFlags = sz53pn_addTable[regB];
                flagQ = true;
                break;
            }
            case 0x41:
            { /* OUT (C),B */
                memptr = getRegBC();
                Z80opsImpl.outPort(memptr++, regB);
                break;
            }
            case 0x42:
            { /* SBC HL,BC */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                sbc16(getRegBC());
                break;
            }
            case 0x43:
            { /* LD (nn),BC */
                memptr = Z80opsImpl.peek16(regPC);
                Z80opsImpl.poke16(memptr++, getRegBC());
                regPC = regPC + 2;
                break;
            }
            case 0x44:
            case 0x4C:
            case 0x54:
            case 0x5C:
            case 0x64:
            case 0x6C:
            case 0x74:
            case 0x7C:
            { /* NEG */
                unsigned int aux = regA;
                regA = 0;
                carryFlag = false;
                sbc(aux);
                break;
            }
            case 0x45:
            case 0x4D: /* RETI */
            case 0x55:
            case 0x5D:
            case 0x65:
            case 0x6D:
            case 0x75:
            case 0x7D:
            { /* RETN */
                ffIFF1 = ffIFF2;
                regPC = memptr = pop();
                break;
            }
            case 0x46:
            case 0x4E:
            case 0x66:
            case 0x6E:
            { /* IM 0 */
                setIM(IntMode::IM0);
                break;
            }
            case 0x47:
            { /* LD I,A */
                /*
                 * El contended-tstate se produce con el contenido de I *antes*
                 * de ser copiado el del registro A. Detalle importante.
                 */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                regI = regA;
                break;
            }
            case 0x48:
            { /* IN C,(C) */
                memptr = getRegBC();
                regC = Z80opsImpl.inPort(memptr++);
                sz5h3pnFlags = sz53pn_addTable[regC];
                flagQ = true;
                break;
            }
            case 0x49:
            { /* OUT (C),C */
                memptr = getRegBC();
                Z80opsImpl.outPort(memptr++, regC);
                break;
            }
            case 0x4A:
            { /* ADC HL,BC */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                adc16(getRegBC());
                break;
            }
            case 0x4B:
            { /* LD BC,(nn) */
                memptr = Z80opsImpl.peek16(regPC);
                setRegBC(Z80opsImpl.peek16(memptr++));
                regPC = regPC + 2;
                break;
            }
            case 0x4F:
            { /* LD R,A */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                setRegR(regA);
                break;
            }
            case 0x50:
            { /* IN D,(C) */
                memptr = getRegBC();
                regD = Z80opsImpl.inPort(memptr++);
                sz5h3pnFlags = sz53pn_addTable[regD];
                flagQ = true;
                break;
            }
            case 0x51:
            { /* OUT (C),D */
                memptr = getRegBC();
                Z80opsImpl.outPort(memptr++, regD);
                break;
            }
            case 0x52:
            { /* SBC HL,DE */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                sbc16(getRegDE());
                break;
            }
            case 0x53:
            { /* LD (nn),DE */
                memptr = Z80opsImpl.peek16(regPC);
                Z80opsImpl.poke16(memptr++, getRegDE());
                regPC = regPC + 2;
                break;
            }
            case 0x56:
            case 0x76:
            { /* IM 1 */
                setIM(IntMode::IM1);
                break;
            }
            case 0x57:
            { /* LD A,I */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                regA = regI;
                sz5h3pnFlags = sz53n_addTable[regA];
                if (ffIFF2) {
                    sz5h3pnFlags |= PARITY_MASK;
                }
                flagQ = true;
                break;
            }
            case 0x58:
            { /* IN E,(C) */
                memptr = getRegBC();
                regE = Z80opsImpl.inPort(memptr++);
                sz5h3pnFlags = sz53pn_addTable[regE];
                flagQ = true;
                break;
            }
            case 0x59:
            { /* OUT (C),E */
                memptr = getRegBC();
                Z80opsImpl.outPort(memptr++, regE);
                break;
            }
            case 0x5A:
            { /* ADC HL,DE */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                adc16(getRegDE());
                break;
            }
            case 0x5B:
            { /* LD DE,(nn) */
                memptr = Z80opsImpl.peek16(regPC);
                setRegDE(Z80opsImpl.peek16(memptr++));
                regPC = regPC + 2;
                break;
            }
            case 0x5E:
            case 0x7E:
            { /* IM 2 */
                setIM(IntMode::IM2);
                break;
            }
            case 0x5F:
            { /* LD A,R */
                Z80opsImpl.contendedStates(getPairIR(), 1);
                regA = getRegR();
                sz5h3pnFlags = sz53n_addTable[regA];
                if (ffIFF2) {
                    sz5h3pnFlags |= PARITY_MASK;
                }
                flagQ = true;
                break;
            }
            case 0x60:
            { /* IN H,(C) */
                memptr = getRegBC();
                regH = Z80opsImpl.inPort(memptr++);
                sz5h3pnFlags = sz53pn_addTable[regH];
                flagQ = true;
                break;
            }
            case 0x61:
            { /* OUT (C),H */
                memptr = getRegBC();
                Z80opsImpl.outPort(memptr++, regH);
                break;
            }
            case 0x62:
            { /* SBC HL,HL */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                sbc16(getRegHL());
                break;
            }
            case 0x63:
            { /* LD (nn),HL */
                memptr = Z80opsImpl.peek16(regPC);
                Z80opsImpl.poke16(memptr++, getRegHL());
                regPC = regPC + 2;
                break;
            }
            case 0x67:
            { /* RRD */
                rrd();
                break;
            }
            case 0x68:
            { /* IN L,(C) */
                memptr = getRegBC();
                regL = Z80opsImpl.inPort(memptr++);
                sz5h3pnFlags = sz53pn_addTable[regL];
                flagQ = true;
                break;
            }
            case 0x69:
            { /* OUT (C),L */
                memptr = getRegBC();
                Z80opsImpl.outPort(memptr++, regL);
                break;
            }
            case 0x6A:
            { /* ADC HL,HL */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                adc16(getRegHL());
                break;
            }
            case 0x6B:
            { /* LD HL,(nn) */
                memptr = Z80opsImpl.peek16(regPC);
                setRegHL(Z80opsImpl.peek16(memptr++));
                regPC = regPC + 2;
                break;
            }
            case 0x6F:
            { /* RLD */
                rld();
                break;
            }
            case 0x70:
            { /* IN (C) */
                memptr = getRegBC();
                uint8_t inPort = Z80opsImpl.inPort(memptr++);
                sz5h3pnFlags = sz53pn_addTable[inPort];
                flagQ = true;
                break;
            }
            case 0x71:
            { /* OUT (C),0 */
                memptr = getRegBC();
                Z80opsImpl.outPort(memptr++, 0x00);
                break;
            }
            case 0x72:
            { /* SBC HL,SP */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                sbc16(regSP);
                break;
            }
            case 0x73:
            { /* LD (nn),SP */
                memptr = Z80opsImpl.peek16(regPC);
                Z80opsImpl.poke16(memptr++, regSP);
                regPC = regPC + 2;
                break;
            }
            case 0x78:
            { /* IN A,(C) */
                memptr = getRegBC();
                regA = Z80opsImpl.inPort(memptr++);
                sz5h3pnFlags = sz53pn_addTable[regA];
                flagQ = true;
                break;
            }
            case 0x79:
            { /* OUT (C),A */
                memptr = getRegBC();
                Z80opsImpl.outPort(memptr++, regA);
                break;
            }
            case 0x7A:
            { /* ADC HL,SP */
                Z80opsImpl.contendedStates(getPairIR(), 7);
                adc16(regSP);
                break;
            }
            case 0x7B:
            { /* LD SP,(nn) */
                memptr = Z80opsImpl.peek16(regPC);
                regSP = Z80opsImpl.peek16(memptr++);
                regPC = regPC + 2;
                break;
            }
            case 0xA0:
            { /* LDI */
                ldi();
                break;
            }
            case 0xA1:
            { /* CPI */
                cpi();
                break;
            }
            case 0xA2:
            { /* INI */
                ini();
                break;
            }
            case 0xA3:
            { /* OUTI */
                outi();
                break;
            }
            case 0xA8:
            { /* LDD */
                ldd();
                break;
            }
            case 0xA9:
            { /* CPD */
                cpd();
                break;
            }
            case 0xAA:
            { /* IND */
                ind();
                break;
            }
            case 0xAB:
            { /* OUTD */
                outd();
                break;
            }
            case 0xB0:
            { /* LDIR */
                ldi();
                if ((sz5h3pnFlags & PARITY_MASK) == PARITY_MASK) {
                    regPC = regPC - 2;
                    memptr = regPC + 1;
                    Z80opsImpl.contendedStates((getRegDE() - 1) & 0xffff, 5);
                }
                break;
            }
            case 0xB1:
            { /* CPIR */
                cpi();
                if ((sz5h3pnFlags & PARITY_MASK) == PARITY_MASK
                        && (sz5h3pnFlags & ZERO_MASK) == 0) {
                    regPC = regPC - 2;
                    memptr = regPC + 1;
                    Z80opsImpl.contendedStates((getRegHL() - 1) & 0xffff, 5);
                }
                break;
            }
            case 0xB2:
            { /* INIR */
                ini();
                if (regB != 0) {
                    regPC = regPC - 2;
                    Z80opsImpl.contendedStates((getRegHL() - 1) & 0xffff, 5);
                }
                break;
            }
            case 0xB3:
            { /* OTIR */
                outi();
                if (regB != 0) {
                    regPC = regPC - 2;
                    Z80opsImpl.contendedStates(getRegBC(), 5);
                }
                break;
            }
            case 0xB8:
            { /* LDDR */
                ldd();
                if ((sz5h3pnFlags & PARITY_MASK) == PARITY_MASK) {
                    regPC = regPC - 2;
                    memptr = regPC + 1;
                    Z80opsImpl.contendedStates((getRegDE() + 1) & 0xffff, 5);
                }
                break;
            }
            case 0xB9:
            { /* CPDR */
                cpd();
                if ((sz5h3pnFlags & PARITY_MASK) == PARITY_MASK
                        && (sz5h3pnFlags & ZERO_MASK) == 0) {
                    regPC = regPC - 2;
                    memptr = regPC + 1;
                    Z80opsImpl.contendedStates((getRegHL() + 1) & 0xffff, 5);
                }
                break;
            }
            case 0xBA:
            { /* INDR */
                ind();
                if (regB != 0) {
                    regPC = regPC - 2;
                    Z80opsImpl.contendedStates((getRegHL() + 1) & 0xffff, 5);
                }
                break;
            }
            case 0xBB:
            { /* OTDR */
                outd();
                if (regB != 0) {
                    regPC = regPC - 2;
                    Z80opsImpl.contendedStates(getRegBC(), 5);
                }
                break;
            }
            default:
            {
                //                System.out.println("Error instrucción ED " + Integer.toHexString(opCode));
                break;
            }
        }
    }
};
/////////////////////////////////////////////////
/////////////////////////////////////////////////

Z80operations::Z80operations() {
    klock = Klock();
    cout << "Terminando constructor de Z80operations" << endl;
}

uint8_t Z80operations::fetchOpcode(uint16_t address) {
    // 3 clocks to fetch opcode from RAM and 1 execution clock
    klock.addTstates(4);
    //cout << "fech opcode from address " << address << endl;
    return z80Ram[address];
}

uint8_t Z80operations::peek8(uint16_t address) {
    klock.addTstates(3); // 3 clocks for read unsigned char from RAM
    return z80Ram[address];
}

void Z80operations::poke8(uint16_t address, uint8_t value) {
    klock.addTstates(3); // 3 clocks for write unsigned char to RAM
    z80Ram[address] = value;
}

uint16_t Z80operations::peek16(uint16_t address) {
    unsigned int lsb = peek8(address);
    unsigned int msb = peek8(address + 1);
    return (msb << 8) | lsb;
}

void Z80operations::poke16(uint16_t address, uint16_t word) {
    poke8(address, word);
    poke8(address + 1, word >> 8);
}

uint8_t Z80operations::inPort(uint16_t port) {
    klock.addTstates(4); // 4 clocks for read unsigned char from bus
    return z80Ports[port];
}

void Z80operations::outPort(uint16_t port, uint8_t value) {
    klock.addTstates(4); // 4 clocks for write unsigned char to bus
    z80Ports[port] = value;
}

void Z80operations::contendedStates(uint16_t address, uint32_t tstates) {
    // Additional clocks to be added on some instructions
    klock.addTstates(tstates);
}

void Z80operations::breakpoint() {
    // Emulate CP/M Syscall at address 5
    switch (z80->getRegC()) {
        case 0: // BDOS 0 System Reset
        {
            cout << "Z80 reset after " << klock.getTstates() << " t-states" << endl;
            finish = true;
            break;
        }
        case 2: // BDOS 2 console char output
        {
            cout << (char) z80->getRegE();
            break;
        }
        case 9: // BDOS 9 console string output (string terminated by "$")
        {
            // cout << "BDOS 9" << endl;
            unsigned int strAddr = z80->getRegDE();
            while (z80Ram[strAddr] != '$') {
                cout << (char) z80Ram[strAddr++];
            }
            break;
        }
        default:
        {
            cout << "BDOS Call " << z80->getRegC() << endl;
            finish = true;
            cout << finish << endl;
        }
    }
}

void Z80operations::execDone() {
}

void Z80operations::runTest(ifstream* f) {
    streampos size;
    if (!f->is_open()) {
        cout << "f NOT OPEN" << endl;
        return;
    } else cout << "f open" << endl;

    size = f->tellg();
    cout << "Test size: " << size << endl;
    f->seekg(0, ios::beg);
    f->read((char *)&z80Ram[0x100], size);
    f->close();

    z80->reset();
    klock.reset();
    finish = false;

    z80Ram[0] = (unsigned char) 0xC3;
    z80Ram[1] = 0x00;
    z80Ram[2] = 0x01; // JP 0x100 CP/M TPA
    z80Ram[5] = (unsigned char) 0xC9; // Return from BDOS call

    z80->setBreakpoint(0x0005, true);
    while (!finish) {
        z80->execute();
    }
}

int main() {
    cout << "main" << endl;
    Z80 cpu = Z80();

    cout << "t1" << endl;
    ifstream f1("zexall.bin", ios::in | ios::binary | ios::ate);
    cpu.Z80opsImpl.runTest(&f1);
    cout << "t1 end" << endl;
    f1.close();

    //     cout << "t2" << endl;
    //     ifstream f2 ("zexdoc.bin", ios::in | ios::binary | ios::ate);
    //     cpu.Z80opsImpl.runTest(&f2);
    //     cout << "t2 end" << endl;
    //     f2.close();

    cout << "done" << endl;
}
