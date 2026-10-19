#pragma once
#include <sys/synchronization.h>
#include <cell/sync2/cond_types.h>
#include <cell/sync2/cond.h>
#include <sys/ppu_thread.h>
#include <string>
#include <vsh/stdc.hpp>

namespace std
{
   /// <summary>
   /// Thread.hpp
   /// </summary>
   class mutex
   {
   public:
       mutex()
       {
           sys_mutex_attribute_initialize(attr);
           errCode = sys_mutex_create(&thisMutex, &attr);
       }
       sys_mutex_t& get_native() { return thisMutex; }
       int errCode;
   private:
       sys_mutex_t thisMutex;
       sys_mutex_attribute_t attr;
   };

   template<class T>
   class lock_guard
   {
   public:
       lock_guard(T& in) : mutex(in)
       {
           sys_mutex_lock(mutex.get_native(), 0);
       }
       ~lock_guard()
       {
           sys_mutex_unlock(mutex.get_native());
       }
   private:
       T& mutex;
   };

}

#define JMJ_THREAD_PRIORITY 1000 // thread priority 0=highest, 3071=lowest
#define JMJ_THREAD_STACK_SIZE 2048 // stack size for newly created threads

namespace jmj
{
    template<typename _Threadable>
    class thread
    {
    public:
        thread() : _joinable(false) {}
        thread(void(_Threadable::* func)(), _Threadable* t, const std::string& name = "jmjthread") : _joinable(true), obj(t), f(func)
        {
            sys_ppu_thread_create(&thread_id, myFunc, reinterpret_cast<uint64_t>(this),
                JMJ_THREAD_PRIORITY, JMJ_THREAD_STACK_SIZE, SYS_PPU_THREAD_CREATE_JOINABLE, name.c_str());
        }
        bool joinable() { return _joinable; }
        void join() { uint64_t rslt; sys_ppu_thread_join(thread_id, &rslt); }
        _Threadable* obj;
    protected:
        bool _joinable;
        sys_ppu_thread_t thread_id;
        void(_Threadable::* f)();
        static void myFunc(uint64_t in) {
            thread* thisObj = reinterpret_cast<thread*>((uint32_t)in);
            (*thisObj->obj.*thisObj->f)();
            sys_ppu_thread_exit(in);
        }
    };

    template <typename T>
    class counting_semaphore
    {
    public:
        counting_semaphore(const T& max = 1) : max(max), count(0)
        {
            // set up the conditional variable
            sys_cond_attribute_t cvAttr;
            sys_cond_attribute_initialize(cvAttr);
            sys_cond_create(&cv, mutex.get_native(), &cvAttr);
        }
        ~counting_semaphore()
        {
            sys_cond_destroy(cv);
        }
        /***
        * Wait for a condition
        */
        void acquire()
        {
            std::lock_guard<std::mutex> lock(mutex);
            while (count <= 0)
            {
                sys_cond_wait(cv, 0);
            }
            count--;
        }
        /****
        * Signal a condition
        */
        void release()
        {
            if (count >= max)
                return;
            count++;
            sys_cond_signal(cv);
        }
    private:
        std::mutex mutex;
        sys_cond_t cv;
        T count;
        T max;
    };

    class binary_semaphore : public counting_semaphore<int>
    {
    public:
        binary_semaphore() : counting_semaphore<int>(1) {}
    };

} // namespace jmj

template<class T>
std::string to_string(T value)
{
   char buf[25];
   vsh::memset(buf, 0, 25);
   int len = vsh::snprintf(buf, sizeof(buf), "%d", value);
   return std::string(buf);
}

template<class T>
std::wstring to_wstring(T value)
{
   wchar_t buf[25];
   vsh::memset(buf, 0, 25);
   int len = vsh::swprintf(buf, sizeof(buf), L"%d", value);
   return std::wstring(buf);
}

template<>
std::string to_string(uint64_t value);

std::string to_string(float value, int decimalPlaces = 0);

std::wstring to_wstring(float value, int decimalPlaces = 0);
