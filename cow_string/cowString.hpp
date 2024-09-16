#ifndef COW_STRING_H
#define COW_STRING_H

#include <iostream>
#include <type_traits>
#include <concepts>
#include <cstring>
#include <iterator> 
#include <cstddef>  
#include <memory>
#include <vector>
#include <codecvt>
#include <locale>

namespace my_impl {

template <typename T, typename U>
requires(std::same_as<T, std::decay_t<U>>)
void construct(T* ptr, U&& rhs) noexcept(noexcept(U{})) 
{new (ptr) T(std::forward<U>(rhs));}

template<typename T> void destroy(T* ptr) noexcept {ptr->~T();}

template<std::forward_iterator It> void destroy(It begin, It end) noexcept {
    while(begin != end) {
        destroy(&*begin++);
    }
}

template <typename T>
struct Iterator final {
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = T*;
    using reference         = T&;

    constexpr Iterator(){};
    constexpr explicit Iterator(pointer ptr) noexcept: ptr_(ptr) {}
    constexpr Iterator(const Iterator& itr) noexcept: ptr_(itr.ptr_) {}
    constexpr Iterator& operator=(const Iterator& itr) noexcept {ptr_ = itr.ptr_;}
    ~Iterator(){}

    constexpr reference operator*() const {return *ptr_;}
    constexpr pointer operator->() const {return ptr_;}

    constexpr Iterator& operator++() {ptr_++; return *this;}
    constexpr Iterator operator++(int) {Iterator tmp = *this; ++(*this); return tmp;}
    constexpr Iterator& operator+=(difference_type n) noexcept {ptr_ += n; return *this;}
    constexpr Iterator operator+(difference_type n) const noexcept {Iterator res(*this); return res += n;}

    friend Iterator operator+(difference_type n, const Iterator& it) noexcept {return it += n;}

    friend bool operator==(const Iterator& lhs, const Iterator& rhs) {return lhs.ptr_ == rhs.ptr_;}
    friend bool operator!=(const Iterator& lhs, const Iterator& rhs) {return lhs.ptr_ != rhs.ptr_;}
    
private:
    pointer ptr_;
};


template <typename T>
struct bufCowString 
{
protected:
    size_t size_, used_ = 0;
    T* data_ = nullptr;
protected:
    bufCowString(size_t n = 0):
        data_(n == 0 ? nullptr : static_cast<T*>(::operator new(sizeof(char)*n))), 
        size_(n) {}

    bufCowString(const bufCowString& buf) = delete;
    bufCowString& operator=(const bufCowString& buf) = delete;

    bufCowString (bufCowString&& buf) noexcept: 
        size_(buf.size_), used_(buf.used_), data_(buf.data_) 
    {
        buf.size_ = 0;
        buf.used_ = 0;
        buf.data_ = nullptr; 
    };
    
    bufCowString& operator=(bufCowString&& buf) noexcept 
    {
        std::swap(data_, buf.data_); 
        std::swap(size_, buf.size_);
        std::swap(used_, buf.used_);
        return *this;
    };

    ~bufCowString() 
    {
        std::destroy(data_, data_ + used_);
        ::operator delete(data_);
    };
};

template<typename CharT, typename Traits = std::char_traits<CharT>> 
requires std::is_same_v<CharT, char>     || 
         std::is_same_v<CharT, char8_t>  ||
         std::is_same_v<CharT, char16_t> ||
         std::is_same_v<CharT, char32_t> ||
         std::is_same_v<CharT, wchar_t>
struct basicCowString final
{
    basicCowString() = default;

    explicit basicCowString(const CharT* str):
        ptr_data_(std::make_shared<buffer>(Traits::length(str))), 
        size_(Traits::length(str))
    {
        std::copy(str, str + Traits::length(str), std::begin(*ptr_data_));
    }

    template <std::contiguous_iterator Iter>
    basicCowString(Iter begin, Iter end):
        ptr_data_(std::make_shared<buffer>(std::distance(begin, end))),
        size_(ptr_data_->size())
    { 
        std::copy(begin, end, std::begin(*ptr_data_));
    }

    basicCowString(const basicCowString& rhs) noexcept = default;
    basicCowString& operator=(const basicCowString& rhs) noexcept = default;

    basicCowString(basicCowString&& rhs) noexcept
    {
        std::swap(ptr_data_, rhs.ptr_data_);
        std::swap(size_, rhs.size_);
    }

    basicCowString& operator=(basicCowString&& rhs) noexcept 
    {
        std::swap(ptr_data_, rhs.ptr_data_);
        std::swap(size_, rhs.size_);
        return *this;
    }

    ~basicCowString() = default;

    size_t size() const noexcept {return size_;}

    bool unique() const noexcept {return ptr_data_.unique();}

    basicCowString substr(size_t begin, size_t end) const {
        if(begin == end) return basicCowString{};

        if(end < begin) throw std::length_error("invalid iterator range");

        basicCowString tmp(std::begin(*ptr_data_)+begin, std::begin(*ptr_data_)+end);
        tmp.detach();
        return tmp;
    }

    friend constexpr bool operator==(const basicCowString& lhs, const basicCowString& rhs) 
    {
        return lhs.size() == rhs.size() && !Traits::compare(lhs.ptr_data_->data(), rhs.ptr_data_->data(), lhs.size());
    }

    friend constexpr bool operator==(const basicCowString& lhs, const CharT* rhs) 
    {
        return lhs.size() == Traits::length(rhs) && !Traits::compare(lhs.ptr_data_->data(), rhs, lhs.size());
    }

    // friend std::ostream& operator<<(std::ostream& os, const basicCowString& string) 
    // {
    //     for(auto&& el : *string.ptr_data_) os << el;
    //     return os;
    // }
private:

    void detach() {
        if(!ptr_data_.unique()) {
            auto new_ptr_data_ = std::make_unique<buffer>(ptr_data_->size());
            std::copy(std::begin(*ptr_data_), std::end(*ptr_data_), std::begin(*new_ptr_data_));
            ptr_data_ = std::move(new_ptr_data_);
        }
    }

    using buffer = std::vector<CharT>;
    std::shared_ptr<buffer> ptr_data_ = nullptr;
    size_t size_ = 0;
};

using cowString     = basicCowString<char>; 
using u8CowString   = basicCowString<char8_t>; 
using u16CowString  = basicCowString<char16_t>; 
using u32CowString  = basicCowString<char32_t>; 
using wCowString    = basicCowString<wchar_t>; 

template <typename T, std::size_t N>
basicCowString<T> makeString(const std::array<T, N>& constexpr_str) {return basicCowString(constexpr_str.data());}

}




#endif //COW_STRING_H