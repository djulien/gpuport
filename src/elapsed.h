//elapsed timer:

#ifndef _ELAPSED_H
#define _ELAPSED_H

 //std::chrono::duration<double> elapsed()
#include <chrono> //now(), duration<>
#include <sstream> //std::stringstream
#include <unistd.h> //getpid()
#include "ostrfmt.h" //FMT()


//#include <iostream> //std::ostringstream, std::ostream
//#include <iomanip> //setfill, setw, etc
double elapsed_msec()
{
    static auto started = std::chrono::high_resolution_clock::now(); //std::chrono::system_clock::now();
//    std::cout << "f(42) = " << fibonacci(42) << '\n';
//    auto end = std::chrono::system_clock::now();
//     std::chrono::duration<double> elapsed_seconds = end-start;    
//    long sec = std::chrono::system_clock::now() - started;
#if 0
    static bool first = true;
    if (first)
    {
        first = false;
        std::ostringstream op;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&started);
//convert 'struct' into 'hex string'
//from https://www.geeksforgeeks.org/conversion-of-struct-data-type-to-hex-string-and-vice-versa/
//void convert_to_hex_string(ostringstream &op, const unsigned char* data, int size)
        std::ostream::fmtflags old_flags = op.flags(); //Format flags
        char old_fill  = op.fill();
        op << std::hex << std::setfill('0'); //Fill characters
        for (int i = 0; i < sizeof(started); i++)
        {
            if (i) op << ' '; //space between two hex values
            op << "0x" << std::setw(2) << static_cast<int>(data[i]); //force output to use hex version of ascii code
        }
        op.flags(old_flags);
        op.fill(old_fill);
        std::cout << "elapsed epoch " << std::hex << op.str() /*time_point*/ << "\n" << std::flush;
    }
#endif
    auto now = std::chrono::high_resolution_clock::now(); //std::chrono::system_clock::now();
//https://stackoverflow.com/questions/14391327/how-to-get-duration-as-int-millis-and-float-seconds-from-chrono
//http://en.cppreference.com/w/cpp/chrono
//    std::chrono::milliseconds msec = std::chrono::duration_cast<std::chrono::milliseconds>(fs);
//    std::chrono::duration<float> duration = now - started;
//    float msec = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
#if 0
    typedef std::chrono::milliseconds ms;
    typedef std::chrono::duration<float> fsec;
    fsec fs = now - started;
    ms d = std::chrono::duration_cast<ms>(fs);
    std::cout << fs.count() << "s\n";
    std::cout << d.count() << "ms\n";
    return d.count();
#endif
    std::chrono::duration<double, std::milli> elapsed = now - started;
//    std::cout << "Waited " << elapsed.count() << " ms\n";
    return elapsed.count();
}


#if 0
class Stopwatch
{
public:
    explicit Stopwatch(/*SrcLine srcline = 0*/): m_started(elapsed_msec()) {} //, m_label(label), m_srcline(NVL(srcline, SRCLINE)) { debug(BLUE_MSG << label << ": in" ENDCOLOR_ATLINE(srcline)); }
    virtual ~Stopwatch() {} //debug(BLUE_MSG << m_label << ": out after %f msec" ENDCOLOR_ATLINE(m_srcline), restart()); }
public: //methods
    double restart() //my_elapsed_msec(bool restart = false)
    {
        double retval = elapsed_msec() - m_started;
        /*if (restart)*/ m_started = elapsed_msec();
        return retval;
    }
private: //data members
    double m_started; //= -elapsed_msec();
};
#endif


std::string timestamp()
{
    std::stringstream ss;
//    ss << thrid;
//    ss << THRID;
//    float x = 1.2;
//    int h = 42;
    ss << FMT("[%4.3f msec") << elapsed_msec();
#ifdef IPC_THREAD
    ss << " " << getpid();
#endif
    ss << "] ";
    return ss.str();
}

#endif //ndef _ELAPSED_H


///////////////////////////////////////////////////////////////////////////////
////
/// Unit tests:
//

#ifdef WANT_UNIT_TEST
#undef WANT_UNIT_TEST //prevent recursion

//#include <iostream> //std::cout, std::flush
#include "debugexc.h" //debug()
#include "msgcolors.h" //*_MSG, ENDCOLOR_*
#include "elapsed.h"

//#ifndef MSG
// #define MSG(msg)  { std::cout << msg << std::flush; }
//#endif

//int main(int argc, const char* argv[])
void unit_test(ARGS& args)
{
    debug(BLUE_MSG << timestamp() << "start" << ENDCOLOR);
    sleep(2); //give parent head start
    debug(GREEN_MSG << timestamp() << "finish" << ENDCOLOR);
//    return 0;
}
#endif //def WANT_UNIT_TEST
//eof