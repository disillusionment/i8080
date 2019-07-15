#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#define NB_ENABLE 0
#define NB_DISABLE 1

int DEBUG = 0;

char OPS_USED[256];

uint8_t FRONT_PANEL_SWITCHES = 0x10;

struct i8080Status {
        /* registers */
        uint8_t b;
        uint8_t c;
        uint8_t d;
        uint8_t e;
        uint8_t h;
        uint8_t l;
        /* accumulator */
        uint8_t a;
        uint8_t psw;
        /* program counter */
        unsigned int pc;
        /* stack poniter */
        uint16_t sp;

        /* processor flags */
        struct {
                unsigned int C  : 1;
                unsigned int const A1 : 1;
                unsigned int P  : 1;
                unsigned int Y  : 1;
                unsigned int I  : 1;
                unsigned int const A0 : 1;
                unsigned int Z  : 1;
                unsigned int S  : 1;
        } flags;
        uint8_t i_en;
} i8080Status;

uint8_t *system_memory;

int halt = 0;
uint32_t temp = 0;
int rom_size; /* for making read only area */
int rom_start;

int resetCPU(struct i8080Status *CPU)
{
        CPU->pc = 0;
        return 0;
}


int kbhit(void)
{
        struct timeval tv;
        fd_set fds;

        tv.tv_sec = 0;
        tv.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds); /* STDIN_FILENO is 0 */
        select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        return FD_ISSET(STDIN_FILENO, &fds);
}


void nonblock(int state)
{
        struct termios ttystate;

        /* get the terminal state */
        tcgetattr(STDIN_FILENO, &ttystate);

        if (state == NB_ENABLE) {
                /*turn off canonical mode */
                /*ttystate.c_lflag &= ~ICANON; */
                ttystate.c_lflag &= ~(ECHO | ECHONL | ICANON);
                /*minimum of number input read. */
                ttystate.c_cc[VMIN] = 1;
        }else if (state == NB_DISABLE) {
                /*turn on canonical mode */
                ttystate.c_lflag |= (ICANON | ECHO);
        }
        /*set the terminal attributes. */
        tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

void dump_registers(struct i8080Status CPUStatus)
{
        printf("Registers b  c  d  e  h  l  a\n");
        printf("          %02x %02x %02x %02x %02x %02x %02x\n", CPUStatus.b, CPUStatus.c, CPUStatus.d, CPUStatus.e, CPUStatus.h, CPUStatus.l, CPUStatus.a);
        printf("          pc          sp\n");
        printf("          $%04x       $%04x\n", CPUStatus.pc, CPUStatus.sp);
        printf("Processor Flags S Z I Y P C\n");
        printf("                %i %i %i %i %i %i\n", CPUStatus.flags.S, CPUStatus.flags.Z, CPUStatus.flags.I, CPUStatus.flags.Y, CPUStatus.flags.P, CPUStatus.flags.C);
}

void dump_ops()
{
        int opct;
        printf("\n");
        for(opct = 0;opct<=256;opct++)
        {
        if(OPS_USED[opct]=='A')
        { printf("%02x ",opct); }
        }
}


void NotImplemented(void)
{
        printf("Instruction Not Implemented.\n\n");
        halt = 1;
}

void write_memory(uint16_t location, uint8_t value)
{
if(location < 0xFFFF) {
/*              printf("Storing $%02x at $%04x\n",value,location); */
                system_memory[location] = value;
}               
/* else { printf("Not storing $%02x at $%04x",value,location);} */
}

void zeroCPU(struct i8080Status *CPU)
{
        CPU->b = 0;
        CPU->c = 0;
        CPU->d = 0;
        CPU->e = 0;
        CPU->h = 0;
        CPU->l = 0;
        CPU->a = 0;
        CPU->pc = 0;
        CPU->sp = 0;

        CPU->flags.S = 0;
        CPU->flags.Z = 0;
        CPU->flags.I = 0;
        CPU->flags.Y = 0;
        CPU->flags.P = 0;
        CPU->flags.C = 0;
}

void check_zero(int reg, struct i8080Status *fCPU)
{
        unsigned int p_temp;
        /* check zero */
        if (reg == 0) {
                fCPU->flags.Z = 1;
        } else {
                fCPU->flags.Z = 0;
        }
        /* check sign */
        if ((reg & 0x80)==0x80) {
                fCPU->flags.S = 1;
        } else {
                fCPU->flags.S = 0;
        }
        /* check parity */
        p_temp = reg;   
        p_temp = p_temp ^ (p_temp >> 4);
        p_temp = p_temp ^ (p_temp >> 2);
        p_temp = p_temp ^ (p_temp >> 1);
        fCPU->flags.P = (~p_temp & 0x01);
}

void dump_mem(int start, int end) {
        int location = 0x00;
        printf("addr $%04x-",start);

        for(location=start;location<end+1;location++) {
/*              printf("$%04x %02x\n",location,system_memory[location]); */
                 printf("%02X",system_memory[location]);
        }
}


uint8_t serial_buffer_in, serial_buffer_out = 0;

void doSerialComm(void)
{
        int i;

/*      printf("Doing serial comms. SB in = %c SB out = %c\n", serial_buffer_in, serial_buffer_out); */
        if (serial_buffer_out) {
                printf("%c", serial_buffer_out);
                fflush(stdout);
                serial_buffer_out = 0;
        }
/*      usleep(1); */
        i = kbhit();
        if (i != 0) {
                serial_buffer_in = fgetc(stdin);

                if(serial_buffer_in == 0x04) {
                        if(DEBUG == 0) {DEBUG = 1; } else { DEBUG = 0; }
                        i=0;serial_buffer_in=0;
                }
                if(serial_buffer_in == 0x18) {
                        dump_mem(0x100,0x120);
                        i=0;serial_buffer_in=0;
                }

                if(serial_buffer_in == 0x19) {
                        dump_ops();     
                        i=0;serial_buffer_in=0;

                }       
        }
}

uint8_t InPort(uint8_t Port)
{
        temp = 0x00;
/*      printf("Reading Byte from Port $%02x\n",Port); */
/*      printf("Buffer is %x,\n",serial_buffer); */
        if (Port == 0x10) {
                if (serial_buffer_in == 0x00) {
                        /*      printf("Have data, ready to send.\n"); */
                        /*      printf("Buffer is %02x\n",serial_buffer); */
                        return(0x02);
                } else {
                        /*      printf("No data, ready to recieve.\n"); */
                        /*      printf("Buffer is %02x\n",serial_buffer); */
                        return(0x01);
                }
        }
        if (Port == 0x11) {
                /* printf("Getting %c from serial.\n",serial_buffer_in); */
                temp = serial_buffer_in;
                /* Patch CR LF */
                if (serial_buffer_in == 0x0a) {
                        temp = 0x0d;
                }
                serial_buffer_in = 0x00;
                return(temp);
        }

/*      SIO Board Status Port and Bits */
        if (Port == 0x00) {
                if (serial_buffer_in != 0x00) {
                        return(0x20);
                } else {
                        return(0x02);
                }
        }


        if (Port == 0x01) {
                /* printf("Getting %c from serial.\n",serial_buffer_in); */
                temp = serial_buffer_in;
                /* Patch CR LF */
                if (serial_buffer_in == 0x0a) {
                        temp = 0x0d;
                }
                serial_buffer_in = 0x00;
                return(temp);
        }

        if (Port == 0xFF) {
                /* Altair/IMSAI Sense Switches */
                /* Set for 2SIO Board with 1 stop Bit */
                return(FRONT_PANEL_SWITCHES);
        }
        return(0);
}

void OutPort(uint8_t Port, uint8_t Byte)
{
        /* printf("Byte $%x02 to Port $%02x\n",Byte, Port); */
        switch (Port) {
        case 0x00:
                break;
        case 0x01:
                serial_buffer_out = Byte & 0x7F;
                break;
        case 0x10:
                /*      printf("Status Port\n"); */
                break;
        case 0x11:
                serial_buffer_out = Byte & 0x7F;
                break;
        default:
                /*      printf("Out port on $%02x, data is %02x ascii %c\n", Port, Byte, Byte); */
                break;
        }
}


void DecodeInstruction(struct i8080Status *CPU)
{
        uint16_t oploc = CPU->pc;
        int addr = 0;

        if (DEBUG) {
                switch (CPU->pc) {
                /* Addresses to ignore */
                case 0x0000:

/* basic.basic.rom * /
                case 0x0043: case 0x0090: case 0x0092: case 0x0094: case 0x0096: case 0x0097: case 0x009b: case 0x009c: case 0x0098:
                case 0x00A3: case 0x00A5: case 0x00A7: case 0x49: case 0xA2: case 0xaa: case 0xab: case 0xad: case 0xae: case 0xaf: case 0xb0: case 0xb1:
*/
/* 4kbas32.bin */                       
case 0x0341: case 0x0382: case 0x0384: case 0x0386: case 0x0389: case 0x038b: case 0x038d: case 0x0344: case 0x0346: case 0x0349: case 0x034b:
case 0x0556: case 0x0558: case 0x055a: 
/* */
                     break;

                default:
        /*              printf("$%04x:", CPU->pc); */
                        printf(" PC $%04x OPCODE= $%02x SP $%04x ", CPU->pc, system_memory[CPU->pc], CPU->sp);
                        printf("BC%02x%02x-DE%02x%02x-HL%02x%02x-A%02x", CPU->b, CPU->c, CPU->d, CPU->e, CPU->h, CPU->l, CPU->a);
                        if(CPU->a > 0x20 && CPU->a < 0x7F) { printf(" \"%c\" ",CPU->a); } else {  printf(" %02x  ",CPU->a); }
                        printf("S%iZ%iI%iY%iP%iC%i ", CPU->flags.S, CPU->flags.Z, CPU->flags.I, CPU->flags.Y, CPU->flags.P, CPU->flags.C);
                        printf("M [$%02x] SP[$%02x%02x]\n",system_memory[(CPU->h*0x100)+CPU->l],system_memory[CPU->sp+1],system_memory[CPU->sp]);
                        break;
                }

                /* printf("Location: %04x  Value %02x \n",CPU->pc,system_memory[CPU->pc]); */
                /*      if(CPU->pc != 0xf699 || CPU->sp != 0xf69b || CPU->sp != 0xf69d) { */
                /*              printf("$%04x:",CPU->pc); */
                /*      } */
        }


        CPU->pc++;
        switch (system_memory[oploc]) {

        case 0x00:
                /* printf("$%04x NOP\n",oploc); */
                break;

        case 0x01:
                /*printf("LXI    B,$%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); */
                CPU->b = system_memory[oploc + 2];
                CPU->c = system_memory[oploc + 1];
                CPU->pc = CPU->pc + 2;
                break;

        case 0x02:
                /*printf("STAX   B addr $%04x, value $%02x\n",(CPU->b*0x100)+CPU->c,CPU->a); */
                system_memory[(CPU->b * 0x100) + CPU->c] = CPU->a;
                break;

        case 0x03:
                /*printf("INX    B\n"); */
                temp = ((CPU->b * 0x100) + CPU->c);
                temp = (temp + 1) & 0xFFFF;
                CPU->c = temp & 0x0FF;
                CPU->b = temp / 0x100;
                break;

        case 0x04:
                /*printf("INR    B\n"); */
                CPU->b = CPU->b + 1;
                check_zero(CPU->b, CPU);
                break;

        case 0x05:
                /*printf("DCR    B\n"); */
                CPU->b = CPU->b - 1;
                check_zero(CPU->b, CPU);
                break;

        case 0x06:
                /*printf("MVI    B,$%02x\n", system_memory[oploc+1]); */
                CPU->b = system_memory[oploc + 1];
                CPU->pc = CPU->pc + 1;
                break;

        case 0x07:
                /*printf("RLC\n"); */
                if ((CPU->a & 0x80) == 0x80) {
                        CPU->flags.C = 1;
                        CPU->a = CPU->a << 1;
                        CPU->a = CPU->a & 0x01;
                } else {
                        CPU->flags.C = 0;
                        CPU->a = CPU->a << 1;
                }
                break;

        case 0x08:
                /*printf("NOP\n"); */
                break;

        case 0x09:
                /*printf("DAD    B\n"); */
                temp = ((CPU->h * 0x100) + CPU->l) + ((CPU->b * 0x100) + CPU->c);
                CPU->h = temp / 0x100;
                CPU->l = temp & 0x0FF;
                CPU->flags.C = (temp >> 16) && 0x1;
                break;

        case 0x0A:
                /*printf("LDAX   B addr $%04x, value $%02x\n",(CPU->b*0x100)+CPU->c,CPU->a); */
                CPU->a = system_memory[(CPU->b * 0x100) + CPU->c];
                break;

        case 0x0B:
                /*printf("DCX    B\n"); */
                temp = ((CPU->b * 0x100) + CPU->c);
                temp = (temp - 1) & 0xFFFF;
                CPU->c = temp & 0x0FF;
                CPU->b = temp / 0x100;
                break;

        case 0x0C:
                /*printf("INR    C\n"); */
                CPU->c = CPU->c + 1;
                check_zero(CPU->c, CPU);
                break;

        case 0x0D:
                /*printf("DCR    C\n"); */
                CPU->c = CPU->c - 1;
                check_zero(CPU->c, CPU);
                break;

        case 0x0E:
                /*printf("MVI    C,$%02x\n", system_memory[oploc+1]); */
                CPU->c = system_memory[oploc + 1];
                CPU->pc = CPU->pc + 1;
                break;

        case 0x0F:
                /*printf("RRC  A=$%02x\n",CPU->a); */
                if (CPU->a & 0x01) {
                        CPU->flags.C = 1;
                        CPU->a = CPU->a >> 1;
                        CPU->a = CPU->a & 0x80;
                } else {
                        CPU->flags.C = 0;
                        CPU->a = CPU->a >> 1;
                }
                break;
        case 0x10:
                /*printf("NOP\n"); */
                break;
        case 0x11:
                /*printf("LXI    D,$%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); */
                CPU->d = system_memory[oploc + 2];
                CPU->e = system_memory[oploc + 1];
                CPU->pc = CPU->pc + 2;
                break;
        case 0x12:
                /*printf("STAX   D addr $%04x, value $%02x\n",(CPU->d*0x100)+CPU->e,CPU->a); */
                system_memory[(CPU->d * 0x100) + CPU->e] = CPU->a;
                break;
        case 0x13:
                /*printf("INX    D\n"); */
                temp = ((CPU->d * 0x100) + CPU->e);
                temp = (temp + 1) & 0xFFFF;
                CPU->e = temp & 0x0FF;
                CPU->d = temp / 0x100;
                break;
        case 0x14:
                /*printf("INR    D\n"); */
                CPU->d = CPU->d + 1;
                check_zero(CPU->d, CPU);
                break;
        case 0x15:
                /*printf("DCR    D\n"); */
                CPU->d = CPU->d - 1;
                check_zero(CPU->d, CPU);
                break;
        case 0x16:
                /*printf("MVI    D,$%02x\n", system_memory[oploc+1]); */
                CPU->d = system_memory[oploc + 1];
                CPU->pc = CPU->pc + 1;
                break;
        case 0x17:
                /*printf("RAL\n"); */
                temp = CPU->a;
                CPU->a =  CPU->flags.C + (temp << 1);
                CPU->flags.C = 0x80 && (temp & 0x80);
                break;
        case 0x18:
                /*printf("NOP\n"); */
                break;
        case 0x19:
                /*printf("DAD    D\n"); */
                temp = ((CPU->h * 0x100) + CPU->l) + ((CPU->d * 0x100) + CPU->e);
                CPU->h = temp / 0x0100;
                CPU->l = temp & 0x00FF;
                CPU->flags.C = (temp >> 16) && 0x1;
                break;
        case 0x1A:
                /*printf("LDAX   D addr $%04x, value $%02x\n",(CPU->d*0x100)+CPU->e,CPU->a); */
                CPU->a = system_memory[(CPU->d * 0x100) + CPU->e];
                break;
        case 0x1B:
                /*printf("DCX    D\n"); */
                temp = ((CPU->d * 0x100) + CPU->e);
                temp = (temp - 1) & 0xFFFF;
                CPU->e = temp & 0x0FF;
                CPU->d = temp / 0x100;
                break;
        case 0x1C:
                /*printf("INR    E\n"); */
                CPU->e = CPU->e + 1;
                check_zero(CPU->e, CPU);
                break;
        case 0x1D:
                /*printf("DCR    E\n"); */
                CPU->e = CPU->e - 1;
                check_zero(CPU->e, CPU);
                break;
        case 0x1E:
                /*printf("MVI    E,$%02x\n", system_memory[oploc+1]); */
                CPU->e = system_memory[oploc + 1];
                CPU->pc = CPU->pc + 1;
                break;
        case 0x1F:
                /*printf("RAR\n"); */
                temp = CPU->a;
                CPU->a = CPU->flags.C << 7 | temp >> 1;
                CPU->flags.C = (0x01 == (temp & 0x01));
                break;
        case 0x20:
                /*printf("NOP\n"); */
                break;
        case 0x21:
                /*printf("LXI    H,$%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); */
                CPU->h = system_memory[oploc + 2];
                CPU->l = system_memory[oploc + 1];
                CPU->pc = CPU->pc + 2;
                break;
        case 0x22:
                /*printf("SHLD   $%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); */
                system_memory[((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1])  ] = CPU->l;
                system_memory[((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]) + 1] = CPU->h;
                CPU->pc = CPU->pc + 2;
                break;
        case 0x23:
                /*printf("INX    H\n"); */
                temp = ((CPU->h * 0x100) + CPU->l);
                temp = (temp + 1) & 0xFFFF;
                CPU->l = temp & 0x0FF;
                CPU->h = temp / 0x100;
                break;
        case 0x24:
                /*printf("INR    H\n"); */
                CPU->h = CPU->h + 1;
                check_zero(CPU->h, CPU);
                break;
        case 0x25:
                /*printf("DCR    H\n"); */
                CPU->h = CPU->h - 1;
                check_zero(CPU->h, CPU);
                break;
        case 0x26:
                /*printf("MVI    H,$%02x\n", system_memory[oploc+1]); */
                CPU->h = system_memory[oploc + 1];
                CPU->pc = CPU->pc + 1;
                break;
        case 0x27:
                /*printf("DAA\n"); */
                temp = CPU->a & 0x0F;
                if ((temp > 0x09) || (CPU->flags.Y == 1)) {
                        CPU->a = CPU->a + 0x06;
                        if (temp > 0x0F) {
                                CPU->flags.Y = 1;
                        } else {
                                CPU->flags.Y = 0;
                        }
                }
                /* printf("Midfunction break Accumulator = %02x\n",CPU->a); */

                if (((CPU->a & 0xF0) > 0x90) || (CPU->flags.C == 1)) {
                        temp = CPU->a + 0x60;
                        if (temp > 0xFF) {
                                CPU->flags.C = 1;
                        } else {
                                CPU->flags.C = 0;
                        }
                        CPU->a = temp & 0xFF;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x28:
                /*printf("NOP\n"); */
                break;
        case 0x29:
                /* printf("DAD    H\n"); */
                temp = ((CPU->h * 0x100) + CPU->l);
                temp = temp + temp;
                CPU->h = temp / 0x100;
                CPU->l = temp & 0x0FF;
                CPU->flags.C = (temp >> 16);
                break;
        case 0x2A:
                /*printf("LHLD   $%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                CPU->l = system_memory[((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1])  ];
                CPU->h = system_memory[((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]) + 1];
                CPU->pc = CPU->pc + 2;
                break;
        case 0x2B:
                /*printf("DCX    H\n"); */
                temp = ((CPU->h * 0x100) + CPU->l);
                temp = (temp - 1) & 0xFFFF;
                CPU->l = temp & 0x0FF;
                CPU->h = temp / 0x100;
                break;
        case 0x2C:
                /*printf("INR    L\n"); */
                CPU->l = CPU->l + 1;
                check_zero(CPU->l, CPU);
                break;
        case 0x2D:
                /*printf("DCR    L\n"); */
                CPU->l = CPU->l - 1;
                check_zero(CPU->l, CPU);
                break;
        case 0x2E:
                /*printf("MVI    L,$%02x\n", system_memory[oploc+1]); */
                CPU->l = system_memory[oploc + 1];
                CPU->pc = CPU->pc + 1;
                break;
        case 0x2F:
                /* printf("CMA\n"); */
                CPU->a = ~CPU->a;
                break;
        case 0x30:
                /*printf("NOP\n"); */
                break;
        case 0x31:
                /*printf("LXI   SP,$%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); */
                CPU->sp = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                CPU->pc = CPU->pc + 2;
                break;
        case 0x32:
                /*printf("STA    $%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; break; */
                addr = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                write_memory(addr, CPU->a); /*system_memory[addr] = CPU->a; */
                CPU->pc = CPU->pc + 2;
                break;
        case 0x33:
                /*printf("INX   SP\n"); */
                temp = CPU->sp;
                temp = (temp + 1) & 0xFFFF;
                CPU->sp = temp;
                break;
        case 0x34:
                /*printf("INR    M\n"); */
                addr = ((CPU->h * 0x100) + CPU->l);
                temp =  system_memory[addr];
                temp = temp + 1;
                write_memory(addr, temp);
                /*system_memory[addr] = system_memory[addr] + 1; */
                check_zero(temp, CPU);
                break;
        case 0x35:
                /*printf("DCR    M\n"); */
                addr = (CPU->h * 0x100) + CPU->l;
                temp =  system_memory[addr];
                temp = temp - 1;
                write_memory(addr, temp);
                /*system_memory[addr] = (system_memory[addr] - 1); */
                check_zero(temp, CPU);
                break;
        case 0x36:
                /*printf("MVI    M,$%02x\n", system_memory[oploc+1]); */
                addr = ((CPU->h * 0x100) + CPU->l);
                write_memory(addr, system_memory[oploc+1]);
                /*system_memory[temp] = system_memory[oploc + 1]; */
                CPU->pc = CPU->pc + 1;
                break;
        case 0x37:
                /*printf("STC\n"); */
                CPU->flags.C = 1;
                break;
        case 0x38:
                /*printf("NOP\n"); */
                break;
        case 0x39:
                /*printf("DAD   SP\n"); */
                temp = ((CPU->h * 0x100) + CPU->l) + (CPU->sp);
                CPU->h = temp / 0x100;
                CPU->l = temp & 0x00FF;
                CPU->flags.C = (temp >> 16);
                break;
        case 0x3A:
                /*printf("LDA    $%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                CPU->a = system_memory[(system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]];
/*              printf("LDA $04x, value = $%02x\n", CPU->a , system_memory[(system_memory[oploc+2]*0x100)+system_memory[oploc+1]]); */
                CPU->pc = CPU->pc + 2;
                break;
        case 0x3B:
                /*printf("DCX   SP\n"); */
                temp = (CPU->sp);
                temp = (temp - 1) & 0xFFFF;
                CPU->sp = temp;
                break;
        case 0x3C:
                /*printf("INR    A\n"); */
                CPU->a = CPU->a + 1;
                check_zero(CPU->a, CPU);
                break;
        case 0x3D:
                /*printf("DCR    A\n"); */
                CPU->a = CPU->a - 1;
                check_zero(CPU->a, CPU);
                break;
        case 0x3E:
                /*printf("MVI    A,$%02x\n", system_memory[oploc+1]); */
                CPU->a = system_memory[oploc + 1];
                CPU->pc = CPU->pc + 1;
                break;
        case 0x3F:
                /*printf("CMC"); */
                CPU->flags.C = ~(CPU->flags.C);
                break;
        case 0x40:
                /*printf("MOV    B,B\n"); */
                CPU->b = CPU->b;
                break;
        case 0x41:
                /*printf("MOV    B,C\n"); */
                CPU->b = CPU->c;
                break;
        case 0x42:
                /*printf("MOV    B,D\n"); */
                CPU->b = CPU->d;
                break;
        case 0x43:
                /*printf("MOV    B,E\n"); */
                CPU->b = CPU->e;
                break;
        case 0x44:
                /*printf("MOV    B,H\n"); */
                CPU->b = CPU->h;
                break;
        case 0x45:
                /*printf("MOV    B,L\n"); */
                CPU->b = CPU->l;
                break;
        case 0x46:
                /*printf("MOV    B,M\n"); */
                CPU->b = system_memory[(CPU->h * 0x100) + CPU->l];
                break;
        case 0x47:
                /*printf("MOV    B,A\n"); */
                CPU->b = CPU->a;
                break;
        case 0x48:
                /*printf("MOV    C,B\n"); */
                CPU->c = CPU->b;
                break;
        case 0x49:
                /*printf("MOV    C,C\n"); */
                CPU->c = CPU->c;
                break;
        case 0x4A:
                /*printf("MOV    C,D\n"); */
                CPU->c = CPU->d;
                break;
        case 0x4B:
                /*printf("MOV    C,E\n"); */
                CPU->c = CPU->e;
                break;
        case 0x4C:
                /*printf("MOV    C,H\n"); */
                CPU->c = CPU->h;
                break;
        case 0x4D:
                /*printf("MOV    C,L\n"); */
                CPU->c = CPU->l;
                break;
        case 0x4E:
                /*printf("MOV    C,M\n"); */
                CPU->c = system_memory[(CPU->h * 0x100) + CPU->l];
                break;
        case 0x4F:
                /*printf("MOV    C,A\n"); */
                CPU->c = CPU->a;
                /*printf("MOV A=%02x to C=$02x\n",CPU->a,CPU->c); */
                break;
        case 0x50:
                /*printf("MOV    D,B\n"); */
                CPU->d = CPU->b;
                break;
        case 0x51:
                /*printf("MOV    D,C\n"); */
                CPU->d = CPU->c;
                break;
        case 0x52:
                /*printf("MOV    D,D\n"); */
                CPU->d = CPU->d;
                break;
        case 0x53:
                /*printf("MOV    D,E\n"); */
                CPU->d = CPU->e;
                break;
        case 0x54:
                /*printf("MOV    D,H\n"); */
                CPU->d = CPU->h;
                break;
        case 0x55:
                /*printf("MOV    D,L\n"); */
                CPU->d = CPU->l;
                break;
        case 0x56:
                /*printf("MOV    D,M\n"); */
                CPU->d = system_memory[(CPU->h * 0x100) + CPU->l];
                break;
        case 0x57:
                /*printf("MOV    D,A\n"); */
                CPU->d = CPU->a;
                break;
        case 0x58:
                /*printf("MOV    E,B\n"); */
                CPU->e = CPU->b;
                break;
        case 0x59:
                /*printf("MOV    E,C\n"); */
                CPU->e = CPU->c;
                break;
        case 0x5A:
                /*printf("MOV    E,D\n"); */
                CPU->e = CPU->d;
                break;
        case 0x5B:
                /*printf("MOV    E,E\n"); */
                CPU->e = CPU->e;
                break;
        case 0x5C:
                /*printf("MOV    E,H\n"); */
                CPU->e = CPU->h;
                break;
        case 0x5D:
                /*printf("MOV    E,L\n"); */
                CPU->e = CPU->l;
                break;
        case 0x5E:
                /*printf("MOV    E,M\n"); */
                CPU->e = system_memory[(CPU->h * 0x100) + CPU->l];
                break;
        case 0x5F:
                /*printf("MOV    E,A"); */
                CPU->e = CPU->a;
                break;
        case 0x60:
                /*printf("MOV    H,B"); */
                CPU->h = CPU->b;
                break;
        case 0x61:
                /*printf("MOV    H,C"); */
                CPU->h = CPU->c;
                break;
        case 0x62:
                /*printf("MOV    H,D"); */
                CPU->h = CPU->d;
                break;
        case 0x63:
                /*printf("MOV    H,E"); */
                CPU->h = CPU->e;
                break;
        case 0x64:
                /*printf("MOV    H,H"); */
                CPU->h = CPU->h;
                break;
        case 0x65:
                /*printf("MOV    H,L"); */
                CPU->h = CPU->l;
                break;
        case 0x66:
                /*printf("MOV    H,M"); */
                CPU->h = system_memory[(CPU->h * 0x100) + CPU->l];
                break;
        case 0x67:
                /*printf("MOV    H,A"); */
                CPU->h = CPU->a;
                break;
        case 0x68:
                /*printf("MOV    L,B"); */
                CPU->l = CPU->b;
                break;
        case 0x69:
                /*printf("MOV    L,C"); */
                CPU->l = CPU->c;
                break;
        case 0x6A:
                /*printf("MOV    L,D"); */
                CPU->l = CPU->d;
                break;
        case 0x6B:
                /*printf("MOV    L,E"); */
                CPU->l = CPU->e;
                break;
        case 0x6C:
                /*printf("MOV    L,H"); */
                CPU->l = CPU->h;
                break;
        case 0x6D:
                /*printf("MOV    L,L"); */
                CPU->l = CPU->l;
                break;
        case 0x6E:
                /*printf("MOV    L,M"); */
                CPU->l = system_memory[(CPU->h * 0x100) + CPU->l];
                break;
        case 0x6F:
                /*printf("MOV    L,A"); */
                CPU->l = CPU->a;
                break;
        case 0x70:
                /*printf("MOV    M,B"); */
                addr = ((CPU->h * 0x100) + CPU->l);
                write_memory(addr, CPU->b);
                break;
        case 0x71:
                /*printf("MOV    M,C"); */
                addr = ((CPU->h * 0x100) + CPU->l);
                write_memory(addr, CPU->c);
                /*system_memory[addr] = CPU->c; */
                break;
        case 0x72:
                /*printf("MOV    M,D"); */
                addr = ((CPU->h * 0x100) + CPU->l);
                write_memory(addr, CPU->d);
                break;
        case 0x73:
                /*printf("MOV    M,E"); */
                addr = ((CPU->h * 0x100) + CPU->l);
                write_memory(addr, CPU->e);
                break;
        case 0x74:
                /*printf("MOV    M,H"); */
                addr = ((CPU->h * 0x100) + CPU->l);
                write_memory(addr, CPU->h);
                break;
        case 0x75:
                /*printf("MOV    M,L"); */
                addr = ((CPU->h * 0x100) + CPU->l);
                write_memory(addr, CPU->l);
                break;
        case 0x76:
                /*printf("HLT\n"); */
                halt = 1;
                break;
        case 0x77:
                /*printf("MOV    M,A\n"); */
                addr = ((CPU->h * 0x100) + CPU->l);
                write_memory(addr, CPU->a);
                break;
        case 0x78:
                /*printf("MOV    A,B"); */
                CPU->a = CPU->b;
                break;
        case 0x79:
                /*printf("MOV    A,C"); */
                CPU->a = CPU->c;
                break;
        case 0x7A:
                /*printf("MOV    A,D"); */
                CPU->a = CPU->d;
                break;
        case 0x7B:
                /*printf("MOV    A,E"); */
                CPU->a = CPU->e;
                break;
        case 0x7C:
                /*printf("MOV    A,H"); */
                CPU->a = CPU->h;
                break;
        case 0x7D:
                /*printf("MOV    A,L"); */
                CPU->a = CPU->l;
                break;
        case 0x7E:
                /*printf("MOV    A,M\n"); */
                /*printf("MOV A, M = $%c\n",system_memory[(CPU->h*0x100)+CPU->l]); */
                CPU->a = system_memory[(CPU->h * 0x100) + CPU->l];
                break;
        case 0x7F:
                /*printf("MOV    A,A"); */
                CPU->a = CPU->a;
                break;
        case 0x80: /*printf("ADD    B\n"); */
                temp = CPU->a + CPU->b;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->b)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x81:
                /*printf("ADD    C\n"); */
                temp = CPU->a + CPU->c;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->c)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x82:
                /*printf("ADD    D"); */
                temp = CPU->a + CPU->d;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->d)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x83:
                /*printf("ADD    E"); */
                temp = CPU->a + CPU->e;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->e)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x84:
                /*printf("ADD    H"); */
                temp = CPU->a + CPU->h;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->h)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x85:
                /*printf("ADD    L"); */
                temp = CPU->a + CPU->l;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->l)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x86:
                /*printf("ADD    M"); */
                temp = CPU->a + (system_memory[(CPU->h * 0x100) + CPU->l]);
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & (system_memory[(CPU->h * 0x100) + CPU->l]))) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x87:
                /*printf("ADD    A"); */
                temp = CPU->a + CPU->a;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->a)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x88:
                /* printf("ADC    B"); */
                temp = CPU->a + CPU->b + CPU->flags.C;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->b)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x89:
                /* printf("ADC    C"); */
                temp = CPU->a + CPU->c + CPU->flags.C;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->c)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x8A:
                /* printf("ADC    D"); */
                temp = CPU->a + CPU->d + CPU->flags.C;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->d)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x8B:
                /* printf("ADC    E"); */
                temp = CPU->a + CPU->e + CPU->flags.C;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->e)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x8C:
                /* printf("ADC    H"); */
                temp = CPU->a + CPU->h + CPU->flags.C;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->h)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x8D:/* printf("ADC    L"); */
                temp = CPU->a + CPU->l + CPU->flags.C;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->l)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x8E:/* printf("ADC    M"); */
                temp = CPU->a + (system_memory[(CPU->h * 0x100) + CPU->l]) + CPU->flags.C;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & (system_memory[(CPU->h * 0x100) + CPU->l]))) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x8F:/* printf("ADC    A"); */
                temp = CPU->a + CPU->a + CPU->flags.C;
                CPU->a = temp & 0xFF;
                CPU->flags.C = temp >> 0x08;
                if (((0x0F & CPU->a) + (0x0F & CPU->a)) > 0x0F) {
                        CPU->flags.Y = 1;
                } else {
                        CPU->flags.Y = 0;
                }
                check_zero(CPU->a, CPU);
                break;
        case 0x90:
                /*printf("SUB    B\n"); */
                CPU->flags.C = (CPU->b > CPU->a);
                CPU->a = CPU->a - CPU->b;
                check_zero(CPU->a, CPU);
                break;
        case 0x91:
                /*printf("SUB    C\n"); */
                CPU->flags.C = (CPU->c > CPU->a);
                CPU->a  = CPU->a - CPU->c;
                check_zero(CPU->a, CPU);
                break;
        case 0x92:
                /*printf("SUB    D\n"); */
                CPU->flags.C = (CPU->d > CPU->a);
                CPU->a = CPU->a - CPU->d;
                check_zero(CPU->a, CPU);
                break;
        case 0x93:
                /*printf("SUB    E\n"); */
                /* printf("A = $%02x,  E=$%02x, A-E = $%02x\n",CPU->a,CPU->e,CPU->a - CPU->e);*/
                CPU->flags.C = (CPU->e > CPU->a);
                CPU->a = CPU->a - CPU->e;
                check_zero(CPU->a, CPU);
                break;
        case 0x94:
                /*printf("SUB    H\n"); */
                CPU->flags.C = (CPU->h > CPU->a);
                CPU->a = CPU->a - CPU->h;
                check_zero(CPU->a, CPU);
                break;
        case 0x95:
                /*printf("SUB    L\n"); */
                CPU->flags.C = (CPU->l > CPU->a);
                CPU->a = CPU->a - CPU->l;
                check_zero(CPU->a, CPU);
                break;
        case 0x96:
                /*printf("SUB    M\n"); */
                CPU->flags.C = (system_memory[(CPU->h * 0x100) + CPU->l] > CPU->a);
                CPU->a = CPU->a - (system_memory[(CPU->h * 0x100) + CPU->l]);
                check_zero(CPU->a, CPU);
                break;
        case 0x97:
                /*printf("SUB    A\n"); */
                CPU->flags.C = (CPU->a > CPU->a);
                CPU->a = CPU->a - CPU->a;
                check_zero(CPU->a, CPU);
                break;
        case 0x98: /*printf("SBB    B"); */
                temp = CPU->a - (CPU->b + CPU->flags.C);
                CPU->flags.C = (temp > 0xFF);
                CPU->a = temp & 0xFF;
                check_zero(CPU->a, CPU);
                break;
        case 0x99: /*printf("SBB    C"); */
                temp = CPU->a - (CPU->c + CPU->flags.C);
                CPU->flags.C = (temp > 0xFF);
                CPU->a = temp & 0xFF;
                check_zero(CPU->a, CPU);
                break;
        case 0x9A: /*printf("SBB    D"); */
                temp = CPU->a - (CPU->d + CPU->flags.C);
                CPU->flags.C = (temp > 0xFF);
                CPU->a = temp & 0xFF;
                check_zero(CPU->a, CPU);
                break;
        case 0x9B: /*printf("SBB    E"); */
                temp = CPU->a - (CPU->e + CPU->flags.C);
                CPU->flags.C = (temp > 0xFF);
                CPU->a = temp & 0xFF;
                check_zero(CPU->a, CPU);
                break;
        case 0x9C: /*printf("SBB    H"); */
                temp = CPU->a - (CPU->h + CPU->flags.C);
                CPU->flags.C = (temp > 0xFF);
                CPU->a = temp & 0xFF;
                check_zero(CPU->a, CPU);
                break;
        case 0x9D: /*printf("SBB    L"); */
                temp = CPU->a - (CPU->l + CPU->flags.C);
                CPU->flags.C = (temp > 0xFF);
                CPU->a = temp & 0xFF;
                check_zero(CPU->a, CPU);
                break;
        case 0x9E: /*printf("SBB    M"); */
                temp = CPU->a - (system_memory[(CPU->h * 0x100) + CPU->l] + CPU->flags.C);
                CPU->flags.C = (temp > 0xFF);
                CPU->a = temp & 0xFF;
                check_zero(CPU->a, CPU);
                break;
        case 0x9F: /*printf("SBB    A"); */
                temp = CPU->a - (CPU->a + CPU->flags.C);
                CPU->flags.C = (temp > 0xFF);
                CPU->a = temp & 0xFF;
                check_zero(CPU->a, CPU);
                break;
        case 0xA0: /*printf("ANA    B"); */
                CPU->a = CPU->a & CPU->b;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xA1: /*printf("ANA    C"); */
                CPU->a = CPU->a & CPU->c;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xA2: /*printf("ANA    D"); */
                CPU->a = CPU->a & CPU->d;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xA3: /*printf("ANA    E"); */
                CPU->a = CPU->a & CPU->e;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xA4: /*printf("ANA    H"); */
                CPU->a = CPU->a & CPU->h;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xA5: /*printf("ANA    L"); */
                CPU->a = CPU->a & CPU->l;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xA6: /*printf("ANA    M"); */
                CPU->a = (CPU->a & (system_memory[(CPU->h * 0x100) + CPU->l]));
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xA7: /*printf("ANA    A"); */
                CPU->a = CPU->a & CPU->a;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xA8: /*printf("XRA    B"); */
                CPU->a = CPU->a ^ CPU->b;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xA9: /*printf("XRA    C"); */
                CPU->a = CPU->a ^ CPU->c;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xAA: /*printf("XRA    D"); */
                CPU->a = CPU->a ^ CPU->d;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xAB: /*printf("XRA    E"); */
                CPU->a = CPU->a ^ CPU->e;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xAC: /*printf("XRA    H"); */
                CPU->a = CPU->a ^ CPU->h;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xAD: /*printf("XRA    L"); */
                CPU->a = CPU->a ^ CPU->l;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xAE: /*printf("XRA    M"); */
                CPU->a = (CPU->a ^ (system_memory[(CPU->h * 0x100) + CPU->l]));
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xAF: /*printf("XRA    A"); */
                CPU->a = CPU->a ^ CPU->a;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                CPU->flags.Y = 0;
                break;
        case 0xB0: /*printf("ORA    B"); */
                CPU->a = (CPU->a | CPU->b);
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xB1: /*printf("ORA    C"); */
                CPU->a = (CPU->a | CPU->c);
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xB2: /*printf("ORA    D"); */
                CPU->a = CPU->a | CPU->d;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xB3: /*printf("ORA    E"); */
                CPU->a = CPU->a | CPU->e;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xB4: /*printf("ORA    H"); */
                CPU->a = CPU->a | CPU->h;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xB5: /*printf("ORA    L"); */
                CPU->a = (CPU->a | CPU->l);
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xB6: /*printf("ORA    M"); */
                CPU->a = CPU->a | (system_memory[(CPU->h * 0x100) + CPU->l]);
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xB7:
                /*printf("ORA    A"); */
                CPU->a = CPU->a | CPU->a;
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                break;
        case 0xB8:
                /*printf("CMP    B\n"); */
                temp = 0;
                temp = (CPU->a - CPU->b);
                if (CPU->a < CPU->b) {
                        CPU->flags.C = 1;
                } else {
                        CPU->flags.C = 0;
                }
                check_zero(temp, CPU);
                break;
        case 0xB9:
                /*printf("CMP    C\n"); */
                temp = 0;
                temp = (CPU->a - CPU->c);
                if (CPU->a < CPU->c) {
                        CPU->flags.C = 1;
                } else {
                        CPU->flags.C = 0;
                }
                check_zero(temp, CPU);
                break;
        case 0xBA: /*printf("CMP    D\n"); */
                temp = 0;
                temp = (CPU->a - CPU->d);
                if (CPU->a < CPU->d) {
                        CPU->flags.C = 1;
                } else {
                        CPU->flags.C = 0;
                }
                check_zero(temp, CPU);
                break;
        case 0xBB: /*printf("CMP    E\n"); */
                temp = 0;
                temp = (CPU->a - CPU->e);
                if (CPU->a < CPU->e) {
                        CPU->flags.C = 1;
                } else {
                        CPU->flags.C = 0;
                }
                check_zero(temp, CPU);
                break;
        case 0xBC: /*printf("CMP    H\n"); */
                temp = 0;
                temp = (CPU->a - CPU->h);
                if (CPU->a < CPU->h) {
                        CPU->flags.C = 1;
                } else {
                        CPU->flags.C = 0;
                }
                check_zero(temp, CPU);
                break;
        case 0xBD: /*printf("CMP    L\n"); */
                temp = 0;
                temp = (CPU->a - CPU->l);
                if (CPU->a < CPU->l) {
                        CPU->flags.C = 1;
                } else {
                        CPU->flags.C = 0;
                }
                check_zero(temp, CPU);
                break;
        case 0xBE: /*printf("CMP    M\n"); */
                temp = 0;
                temp = CPU->a - system_memory[(CPU->h * 0x100) + CPU->l];
                if ((CPU->a < system_memory[(CPU->h * 0x100) + CPU->l])) {
                        CPU->flags.C = 1;
                } else {
                        CPU->flags.C = 0;
                }
                check_zero(temp, CPU);
                break;
        case 0xBF: /*printf("CMP    A\n"); */
                temp = 0;
                temp = (CPU->a - CPU->a);
                CPU->flags.C = 0;
                check_zero(temp, CPU);
                break;
        case 0xC0: /*printf("RNZ"); */
                if (CPU->flags.Z == 0) {
                        CPU->pc = (system_memory[CPU->sp + 1] * 0x100) + system_memory[CPU->sp];
                        CPU->sp = CPU->sp + 2;
                }
                break;
        case 0xC1: /*printf("POP    B"); */
                CPU->b = system_memory[CPU->sp + 1];
                CPU->c = system_memory[CPU->sp];
                CPU->sp = CPU->sp + 2;
                break;
        case 0xC2:
                /*printf("JNZ    $%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); */
                if (CPU->flags.Z == 0) {
                        CPU->pc = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xC3:
                /*printf("$%04x JMP    $%02x%02x\n", CPU->pc,system_memory[oploc+2], system_memory[oploc+1]); */
                CPU->pc = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                break;
        case 0xC4:                                                              /*printf("CNZ    $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); */
                if (CPU->flags.Z == 0) {
                        system_memory[CPU->sp - 1] = ((oploc + 3) / 0x100);     /*//printf("%04x\n",((CPU->pc+3) / 0x100)); */
                        system_memory[CPU->sp - 2] = ((oploc + 3) & 0xFF);      /*//printf("%04x\n",(( CPU->pc+3 )& 0xFF)); */
                        CPU->sp = CPU->sp - 2;                                  /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                        CPU->pc = ((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]);
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xC5:
                /*printf("PUSH   B"); */
                system_memory[CPU->sp - 1] = CPU->b;
                system_memory[CPU->sp - 2] = CPU->c;
                CPU->sp = CPU->sp - 2;
                break;
        case 0xC6: /*printf("ADI    $%02x", system_memory[oploc+1]); */
                temp = CPU->a + system_memory[oploc + 1];
                if (temp > 0xff) {
                        CPU->flags.C = 1;
                } else {
                        CPU->flags.C = 0;
                }
                CPU->a = temp & 0xFF;
                check_zero(CPU->a, CPU);
                CPU->pc = CPU->pc + 1;
                break;
        case 0xC7: /*printf("RST    0"); */
                system_memory[CPU->sp - 1] = ((oploc + 1) / 0x100);
                system_memory[CPU->sp - 2] = ((oploc + 1) & 0xFF);
                CPU->sp = CPU->sp - 2;
                CPU->pc = (0x0000);
                break;
        case 0xC8: /*printf("RZ"); */
                if (CPU->flags.Z == 1) {
                        CPU->pc = (system_memory[CPU->sp + 1] * 0x100) + system_memory[CPU->sp];
                        CPU->sp = CPU->sp + 2;
                }
                break;
        case 0xC9:
                /*printf("RET\n"); */
                /* Restore Address from Stack */
                CPU->pc = (system_memory[CPU->sp + 1] * 0x100) + system_memory[CPU->sp];
                CPU->sp = CPU->sp + 2;
                /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                break;

        case 0xCA: /*printf("JZ     $%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                if (CPU->flags.Z == 1) {
                        CPU->pc = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                } else {
                        CPU->pc = CPU->pc + 2;
                }

                break;
        case 0xCB:
                /*printf("NOP\n"); */
                break;
        case 0xCC:
                /*printf("CZ     $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); */
                if (CPU->flags.Z == 1) {
                        system_memory[CPU->sp - 1] = ((oploc + 3) / 0x100);     /*//printf("%04x\n",((CPU->pc+3) / 0x100)); */
                        system_memory[CPU->sp - 2] = ((oploc + 3) & 0xFF);      /*//printf("%04x\n",(( CPU->pc+3 )& 0xFF)); */
                        CPU->sp = CPU->sp - 2;                                  /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                        CPU->pc = ((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]);
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xCD:
                /*printf("CALL   $%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); */
                /*//printf("Stack Pointer Starts at: $%04x\n",CPU->sp); */
                system_memory[CPU->sp - 1] = ((oploc + 3) / 0x100);     /*//printf("%04x\n",((CPU->pc+3) / 0x100)); */
                system_memory[CPU->sp - 2] = ((oploc + 3) & 0xFF);      /*//printf("%04x\n",(( CPU->pc+3 )& 0xFF)); */
                CPU->sp = CPU->sp - 2;                                  /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                CPU->pc = ((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]);
                break;
        case 0xCE: /*printf("ACI    $%02x", system_memory[oploc+1]); */
                temp = CPU->a + system_memory[oploc + 1] + CPU->flags.C;
                if (temp > 0xff) {
                        CPU->flags.C = 1;
                } else {
                        CPU->flags.C = 0;
                }
                CPU->a = temp & 0xFF;
                check_zero(CPU->a, CPU);
                CPU->pc = CPU->pc + 1;
                break;
        case 0xCF: /*printf("RST    1"); */
                system_memory[CPU->sp - 1] = ((oploc + 1) / 0x100);
                system_memory[CPU->sp - 2] = ((oploc + 1) & 0xFF);
                CPU->sp = CPU->sp - 2;
                CPU->pc = 0x0008;
                break;
        case 0xD0: /*printf("RNC"); */
                if (CPU->flags.C == 0) {
                        CPU->pc = (system_memory[CPU->sp + 1] * 0x100) + system_memory[CPU->sp];
                        CPU->sp = CPU->sp + 2;
                }
                break;
        case 0xD1: /*printf("POP    D"); */
                CPU->d = system_memory[CPU->sp + 1];
                CPU->e = system_memory[CPU->sp];
                CPU->sp = CPU->sp + 2;
                break;
        case 0xD2:
                /*printf("JNC    $%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); */
                if (CPU->flags.C == 0) {
                        CPU->pc = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xD3:
                /*printf("OUT    $%02x, $%02x\n", system_memory[oploc+1],CPU->a); */
                OutPort(system_memory[oploc + 1], CPU->a);
                CPU->pc = CPU->pc + 1;
                break;
        case 0xD4:
                /*printf("CNC    $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                if (CPU->flags.C == 0) {
                        system_memory[CPU->sp - 1] = ((oploc + 3) / 0x100);     /*//printf("%04x\n",((CPU->pc+3) / 0x100)); */
                        system_memory[CPU->sp - 2] = ((oploc + 3) & 0xFF);      /*//printf("%04x\n",(( CPU->pc+3 )& 0xFF)); */
                        CPU->sp = CPU->sp - 2;                                  /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                        CPU->pc = ((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]);
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xD5:
                /*printf("PUSH   D"); */
                system_memory[CPU->sp - 1] = CPU->d;
                system_memory[CPU->sp - 2] = CPU->e;
                CPU->sp = CPU->sp - 2;
                break;
        case 0xD6: /*printf("SUI    $%02x", system_memory[oploc+1]); opbytes=1; */
                temp = CPU->a - (system_memory[oploc + 1]);
                CPU->a = temp & 0xFF;
                CPU->flags.C = (temp > 0xFF);
                check_zero(CPU->a, CPU);
                CPU->pc = CPU->pc + 1;
                break;
        case 0xD7: /* printf("RST    2"); */
                system_memory[CPU->sp - 1] = ((oploc + 1) / 0x100);
                system_memory[CPU->sp - 2] = ((oploc + 1) & 0xFF);
                CPU->sp = CPU->sp - 2;
                CPU->pc = (0x0010);
                break;
        case 0xD8: /*printf("RC"); */
                if (CPU->flags.C == 1) {
                        CPU->pc = (system_memory[CPU->sp + 1] * 0x100) + system_memory[CPU->sp];
                        CPU->sp = CPU->sp + 2;
                }
                break;
        case 0xD9:
                /*printf("NOP\n"); */
                break;
        case 0xDA:
                /*printf("JC     $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                if (CPU->flags.C == 1) {
                        CPU->pc = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xDB:
                /*printf("IN     $%02x", system_memory[oploc+1]); */
                /*//printf("Device $%02x: Value $%02x\n",system_memory[oploc+1], CPU->a); */
                CPU->a = InPort(system_memory[oploc + 1]);
                CPU->pc++;
                break;
        case 0xDC:
                /* printf("CC     $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                if (CPU->flags.C == 1) {
                        system_memory[CPU->sp - 1] = ((oploc + 3) / 0x100);     /*//printf("%04x\n",((CPU->pc+3) / 0x100)); */
                        system_memory[CPU->sp - 2] = ((oploc + 3) & 0xFF);      /*//printf("%04x\n",(( CPU->pc+3 )& 0xFF)); */
                        CPU->sp = CPU->sp - 2;                                  /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                        CPU->pc = ((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]);
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xDD:
                /*printf("NOP\n"); */
                break;
        case 0xDE:
                /* printf("SBI    $%02x", system_memory[oploc+1]); opbytes=2; */
                /*2-18-25 11pm was subtractic CPU->b */
                temp = CPU->a + (~(system_memory[oploc+1] + CPU->flags.C) + 0x01);
                CPU->flags.C = (temp > 0xFF);
                CPU->flags.Y = 0;
                check_zero(CPU->a, CPU);
                CPU->pc++;
                break;
        case 0xDF:
                /* printf("RST    3"); */
                system_memory[CPU->sp - 1] = ((oploc + 1) / 0x100);
                system_memory[CPU->sp - 2] = ((oploc + 1) & 0xFF);
                CPU->sp = CPU->sp - 2;
                CPU->pc = (0x0018);
                break;
        case 0xE0:
                /*printf("RPO"); */
                if (CPU->flags.P == 0) {
                        CPU->pc = (system_memory[CPU->sp + 1] * 0x100) + system_memory[CPU->sp];
                        CPU->sp = CPU->sp + 2;
                }
                break;
        case 0xE1: /*printf("POP    H"); */
                CPU->h = system_memory[CPU->sp + 1];
                CPU->l = system_memory[CPU->sp];
                CPU->sp = CPU->sp + 2;
                break;
        case 0xE2:
                /*printf("JPO    $%02x%02x\n", system_memory[oploc+2], system_memory[oploc+1]); */
                if (CPU->flags.P == 0) {
                        CPU->pc = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xE3: /*printf("XTHL\n"); */
                   /*printf("Swapping HL = %04x with SP %04x. \n",((CPU->h*0x100)+CPU->l),((system_memory[CPU->sp+1]*0x100)+(system_memory[CPU->sp]))); */
                temp = system_memory[CPU->sp];
                system_memory[CPU->sp] = CPU->l;
                CPU->l = temp;
                temp = system_memory[CPU->sp + 1];
                system_memory[CPU->sp + 1] = CPU->h;
                CPU->h = temp;
                temp = 0x00;
                break;
        case 0xE4:
                /* printf("CPO    $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                if (CPU->flags.P == 0) {
                        system_memory[CPU->sp - 1] = ((CPU->pc + 2) / 0x100);   /*//printf("%04x\n",((CPU->pc+3) / 0x100)); */
                        system_memory[CPU->sp - 2] = ((CPU->pc + 2) & 0xFF);    /*//printf("%04x\n",(( CPU->pc+3 )& 0xFF)); */
                        CPU->sp = CPU->sp - 2;                                  /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                        CPU->pc = ((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]);
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xE5:
                /*printf("PUSH   H"); */
                system_memory[CPU->sp - 1] = CPU->h;
                system_memory[CPU->sp - 2] = CPU->l;
                CPU->sp = CPU->sp - 2;
                break;
        case 0xE6:
                /*printf("ANI    $%02x\n", system_memory[oploc+1]); */
                /*printf("ANI a=%02x, I=%02x, Result = %02x \n",CPU->a,system_memory[oploc+1], ( CPU->a & system_memory[oploc+1] )); */
                CPU->a = (CPU->a & system_memory[oploc + 1]);
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                CPU->flags.Y = 0;
                CPU->pc++;
                break;
        case 0xE7:/* printf("RST    4"); */
                system_memory[CPU->sp - 1] = ((oploc + 1) / 0x100);
                system_memory[CPU->sp - 2] = ((oploc + 1) & 0xFF);
                CPU->sp = CPU->sp - 2;
                CPU->pc = (0x0020);
                break;
        case 0xE8:/* printf("RPE"); */
                if (CPU->flags.P == 1) {
                        CPU->pc = (system_memory[CPU->sp + 1] * 0x100) + system_memory[CPU->sp];
                        CPU->sp = CPU->sp + 2;
                }
                break;
        case 0xE9:
                /*printf("PCHL"); */
                CPU->pc = ((CPU->h * 0x100) + CPU->l);
                break;
        case 0xEA: /*printf("JPE    $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                if (CPU->flags.P == 1) {
                        CPU->pc = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xEB: /*printf("XCHG"); */
                temp = CPU->d; CPU->d = CPU->h; CPU->h = temp;
                temp = CPU->e; CPU->e = CPU->l; CPU->l = temp;
                /*printf("HL = %04x, DE = %04x\n",((CPU->h*0x100)+CPU->l),((CPU->d*0x100)+CPU->e)); */
                temp = 0x00;
                break;
        case 0xEC:
                /* printf("CPE    $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                if (CPU->flags.P == 1) {
                        system_memory[CPU->sp - 1] = ((oploc + 3) / 0x100);     /*//printf("%04x\n",((CPU->pc+3) / 0x100)); */
                        system_memory[CPU->sp - 2] = ((oploc + 3) & 0xFF);      /*//printf("%04x\n",(( CPU->pc+3 )& 0xFF)); */
                        CPU->sp = CPU->sp - 2;                                  /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                        CPU->pc = ((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]);
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xED:
                /*printf("NOP\n"); */
                break;
        case 0xEE:
                /*printf("XRI    $%02x\n", system_memory[oploc+1]); */
                /*printf("XRI a=%02x, I=%02x\n",CPU->a,system_memory[oploc+1]); */
                CPU->a = (CPU->a ^ system_memory[oploc + 1]);
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                CPU->flags.Y = 0;
                CPU->pc = CPU->pc + 1;
                break;
        case 0xEF:/* printf("RST    5"); */
                system_memory[CPU->sp - 1] = ((oploc + 1) / 0x100);
                system_memory[CPU->sp - 2] = ((oploc + 1) & 0xFF);
                CPU->sp = CPU->sp - 2;
                CPU->pc = (0x0028);
                break;
        case 0xF0:
                /* printf("RP"); */
                if (CPU->flags.S == 0) {
                        CPU->pc = (system_memory[CPU->sp + 1] * 0x100) + system_memory[CPU->sp];
                        CPU->sp = CPU->sp + 2;
                }
                break;
        case 0xF1:
                /*printf("POP    PSW\n"); */
                temp = 0x00;
                /* Restore A from Stack */
                CPU->a = system_memory[CPU->sp + 1];
                /* Restore PSW from stack */
                (CPU->flags.C) = (system_memory[CPU->sp] & 0x01);
                /* */
                (CPU->flags.P) = (system_memory[CPU->sp] & 0x04) >> 2;
                (CPU->flags.Y) = (system_memory[CPU->sp] & 0x08) >> 3;
                (CPU->flags.I) = (system_memory[CPU->sp] & 0x10) >> 4;
                /* */
                (CPU->flags.Z) = (system_memory[CPU->sp] & 0x40) >> 6;
                (CPU->flags.S) = (system_memory[CPU->sp] & 0x80) >> 7;

                /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                CPU->sp = CPU->sp + 2;
                break;

        case 0xF2:
                /*printf("JP     $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                if (CPU->flags.S == 0) {
                        CPU->pc = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;
        case 0xF3:
                /*printf("DI"); */
                CPU->i_en = 0;
                break;
        case 0xF4:
                /* printf("CP     $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); */
                if (CPU->flags.S == 0) {
                        system_memory[CPU->sp - 1] = ((oploc + 3) / 0x100);     /*//printf("%04x\n",((CPU->pc+3) / 0x100)); */
                        system_memory[CPU->sp - 2] = ((oploc + 3) & 0xFF);      /*//printf("%04x\n",(( CPU->pc+3 )& 0xFF)); */
                        CPU->sp = CPU->sp - 2;                                  /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                        CPU->pc = ((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]);
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;

        case 0xF5:
                /*printf("PUSH   PSW\n"); */
                system_memory[CPU->sp - 1] = CPU->a;
                temp = (CPU->flags.C) + (CPU->flags.A1 << 1) + (CPU->flags.P << 2) + (CPU->flags.Y << 4) + (CPU->flags.I << 5) + (CPU->flags.Z << 6) + (CPU->flags.S << 7);
                system_memory[CPU->sp - 2] = temp;
                CPU->sp = CPU->sp - 2;
                break;

        case 0xF6:
                /*printf("ORI    $%02x\n", system_memory[oploc+1]); opbytes=1; */
                CPU->a = (CPU->a | system_memory[oploc + 1]);
                check_zero(CPU->a, CPU);
                CPU->flags.C = 0;
                CPU->flags.Y = 0;
                CPU->pc = CPU->pc + 1;
                break;

        case 0xF7:/* printf("RST    6"); */
                system_memory[CPU->sp - 1] = ((oploc + 1) / 0x100);
                system_memory[CPU->sp - 2] = ((oploc + 1) & 0xFF);
                CPU->sp = CPU->sp - 2;
                CPU->pc = (0x0030);
                break;

        case 0xF8:/* printf("RM"); */
                if (CPU->flags.S == 1) {
                        CPU->pc = (system_memory[CPU->sp + 1] * 0x100) + system_memory[CPU->sp];
                        CPU->sp = CPU->sp + 2;
                }
                break;

        case 0xF9:
                /*printf("SPHL"); */
                CPU->sp = ((CPU->h * 0x100) + CPU->l);
                break;

        case 0xFA: /*printf("JM     $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); opbytes=3; */
                if (CPU->flags.S == 1) {
                        CPU->pc = (system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1];
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;

        case 0xFB: printf("EI");
                NotImplemented();
                break;

        case 0xFC:
                /*printf("CM     $%02x%02x", system_memory[oploc+2], system_memory[oploc+1]); */
                if (CPU->flags.S == 1) {
                        system_memory[CPU->sp - 1] = ((oploc + 3) / 0x100);     /*//printf("%04x\n",((CPU->pc+3) / 0x100)); */
                        system_memory[CPU->sp - 2] = ((oploc + 3) & 0xFF);      /*//printf("%04x\n",(( CPU->pc+3 )& 0xFF)); */
                        CPU->sp = CPU->sp - 2;                                  /*//printf("Stack Pointer Now: $%04x\n",CPU->sp); */
                        CPU->pc = ((system_memory[oploc + 2] * 0x100) + system_memory[oploc + 1]);
                } else {
                        CPU->pc = CPU->pc + 2;
                }
                break;

        case 0xFD:
                /*printf("NOP\n"); */
                break;

        case 0xFE:
                /*printf("CPI    $%02x", system_memory[oploc+1]); */
                { uint8_t cpi_temp;
                cpi_temp = (CPU->a - (system_memory[oploc + 1]));
                if ((system_memory[oploc + 1]) > CPU->a) {
                        CPU->flags.C = 1;
                } else {
                        CPU->flags.C = 0;
                }

                /* CPU->flags.C = ~(temp && 0x10000); */
                check_zero(cpi_temp, CPU);
                /* if((oploc != 0x0349) && (oploc != 0x0344) &&  (oploc != 0x09DD)) {
                         printf("PC = $%04x - Comparing $%02x to $%02x : %c to %c result is %02x\n", CPU->pc, CPU->a, (system_memory[oploc+1]), CPU->a, (system_memory[oploc+1]),cpi_temp);
                }*/
                CPU->pc = CPU->pc + 1; }
                break;

        case 0xFF: /*printf("RST    7\n"); */
                system_memory[CPU->sp - 1] = ((oploc + 1) / 0x100);
                system_memory[CPU->sp - 2] = ((oploc + 1) & 0xFF);
                CPU->sp = CPU->sp - 2;
                CPU->pc = (0x0038);
                break;
        }
}

int load_memory(char *filename, uint16_t location)
{
        /* load file from commandline */

        uint8_t *buffer = &system_memory[location];
        FILE *fp = fopen(filename, "rb");
        int fsize;

        if (fp == NULL) {
                printf("error: Couldn't open %s\n", filename);
                exit(1);
        }

        fseek(fp, 0L, SEEK_END);
        fsize = ftell(fp);
        fseek(fp, 0L, SEEK_SET);

        printf("Reading file into memory: %i bytes.\n", fsize);
        fread(buffer, fsize, 1, fp);
        printf("File Read into Memory.\n");
        fclose(fp);
        printf("File Closed\n");
        return(fsize); 
}


int main(int argc, char**argv)
{
        struct i8080Status MyCPU;
        uint16_t start_location;

        if (!argv[2]) {
                printf("No memory location given.  Defaulting to 0x0000.\n");
                start_location = 0x0000;
        } else {
                printf("%s ", argv[2]);
                start_location = (int)strtol(argv[2], NULL, 16);
        }
        if(argc > 2) {
        if (!argv[3]) {
                printf("No front panel settings given.  Defaulting to 0.\n");
                FRONT_PANEL_SWITCHES = 0x00;
        } else {
                printf("%s\n", argv[3]);
                FRONT_PANEL_SWITCHES = (int)strtol(argv[2], NULL, 16);
        }
        }

        printf("Loading file to $%04x\n\n", start_location);

        zeroCPU(&MyCPU);
        system_memory = malloc(0xFFFF);
        rom_size = load_memory(argv[1], start_location);
        printf("RomSize=$%04x\n",rom_size);
        rom_start = start_location;
        MyCPU.pc = start_location;
        /*dump_registers(MyCPU); */
        nonblock(NB_ENABLE);
        printf("\033[2J\033[1;1H");
        while (!halt) {
/*              printf("\033[2J\033[1;1H"); */
                /* usleep(1000); */
                doSerialComm();
/*              printf(" \b"); */
                DecodeInstruction(&MyCPU);
                /*dump_registers(MyCPU); */
                }


        nonblock(NB_DISABLE);
        printf("Halting for some reason.\n");
        dump_registers(MyCPU);
        return 0;
}
