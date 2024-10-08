#ifndef RAM_H
#define RAM_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <numeric>
#include <exception>
#include "token.hpp"
#include <sstream>

namespace my_impl {

constexpr size_t MEM_SIZE = 1000;

template <typename T, typename U>
inline void print(const std::unordered_map<T, U>& map) {
    for(auto&& [key, value] : map) {
        std::cout << "key: " << key 
        << " with value: " << value <<  std::endl;
    }
    
}

inline std::unordered_map<std::string, int> var_store_;
inline std::vector<int> global_memory_(MEM_SIZE);

class RamParser final {
public:
    RamParser():out_stream_(std::make_unique<std::stringstream>())
    {
        std::iota(std::begin(global_memory_), std::end(global_memory_), 0);
    }

    void pushToken(std::unique_ptr<iToken> token) 
    {
        tokens_.emplace_back(std::move(token));
    }

    void parse() {
        c_it_ = std::cbegin(tokens_);
        statement();
    }
    
    std::unique_ptr<std::stringstream> getStream() noexcept {
        return std::move(out_stream_);
    }

    friend std::ostream& operator<<(std::ostream& os, const RamParser& parser) 
    {
        for(auto&& token : parser.tokens_) {
            os << token->toString() << ' ';
        }
        return os;
    }
private:
    std::unique_ptr<std::stringstream> out_stream_;
    using tokens_t =  std::vector<std::unique_ptr<iToken>>;
    using tokens_cit = tokens_t::const_iterator;
    tokens_t tokens_;
    tokens_cit c_it_;

    void nextToken() noexcept {++c_it_;};

    int statement() 
    {
        if(c_it_ == std::cend(tokens_)) return 0;

        switch ((*c_it_)->type())
        {
        case token_type::INPUT:
            input();
            break;
        case token_type::OUTPUT:
            output();
            break;
        case token_type::ID:
            assign(static_cast<IdToken&>(**c_it_).id());
            break;
        case token_type::OBRAC:
            if(**std::next(c_it_) == token_type::ASSIGN) 
            {
                assign("c");
            }
            break;
        case token_type::CBRAC:
            if(**std::next(c_it_) == token_type::OBRAC &&
                **std::next(c_it_, 2) == token_type::ASSIGN)
            {
                nextToken();
                assign("x");
            }
            break;
        default:
            break;
        }
        nextToken();
        if(**c_it_ == token_type::SCOLON) {
            nextToken();
            return statement();
        }
        throw std::runtime_error("Syntax error: expected ;");
    }

    int input() 
    {
        nextToken();
        if(**c_it_ == token_type::ID) {

            auto id = static_cast<IdToken&>(**c_it_).id();

            int tmp = 0;
            std::cin >> tmp;
            var_store_.emplace(id, tmp);
        }
        return 0;
    }

    int output() 
    {
        nextToken();
        if(**c_it_ == token_type::ID) {
            auto var = var_store_.at(static_cast<IdToken&>(**c_it_).id());   
            *out_stream_ << var << std::endl;  
        }
        else if(**c_it_ == token_type::OBRAC &&
                **std::next(c_it_) == token_type::SCOLON) {
            auto var = var_store_.at("c");
            *out_stream_ << var << std::endl;
        }
        else if(**c_it_ == token_type::CBRAC &&
                **std::next(c_it_) == token_type::OBRAC &&
                **std::next(c_it_, 2) == token_type::SCOLON) {
            auto var = var_store_.at("x");
            *out_stream_ << var << std::endl;
            nextToken();
        }
        else {
            auto var = expr();   
            *out_stream_ << var << std::endl;
        }

        return 0;
    }

    int assign(std::string id) {
        nextToken();

        if(**c_it_ == token_type::ASSIGN) {
            if(var_store_.find(id) != std::end(var_store_)) {
                nextToken();

                int res = expr();
                if(res < 0) throw std::runtime_error("Negative index");
                
                var_store_.at(id) = global_memory_.at(res);
            }
            else {
                nextToken();

                int res = expr();
                if(res < 0) throw std::runtime_error("Negative index");
                var_store_.emplace(id, global_memory_.at(res));
            }
        }
        return 0;
    }

    int expr() {
        if (emptyExpr(c_it_)) throw std::runtime_error("Syntax error");

        int res{};
        // id[expr]
        if(**c_it_ == token_type::ID && 
           **std::next(c_it_) == token_type::OBRAC) 
        {
            auto id = static_cast<IdToken&>(**c_it_).id();
            nextToken();
            nextToken();
            auto tmp = expr();
            if(**c_it_ == token_type::CBRAC)
            {
                return var_store_.at(id) + tmp;
            }
        }
        //expr
        else {
            auto tmp = factor();
            while(**c_it_ == token_type::ADD ||
                  **c_it_ == token_type::SUB)
            {
                switch ((*c_it_)->type())
                {
                case token_type::ADD:
                    nextToken();
                    tmp += expr();
                    break;
                case token_type::SUB:
                    nextToken();
                    tmp -= expr();
                    break;
                default:
                    break;
                }
            }
            res = tmp;
        }
        return res;
    }

    int factor() {
        auto res = 0;
        //[expr]
        if(notC(c_it_))
        {
            nextToken();
            auto tmp = expr();
            if(**c_it_ == token_type::CBRAC && //end of expr
               **std::next(c_it_) == token_type::SCOLON)
            {
                return tmp;
            }
            res = tmp;
        }
        else if(**c_it_ == token_type::VALUE) {
            res = static_cast<ValueToken&>(**c_it_).value();
        }
        else if(**c_it_ == token_type::ID) {
            res = var_store_.at(static_cast<IdToken&>(**c_it_).id());
        }
        // [ as c
        else if(**c_it_ == token_type::OBRAC) {
            res = var_store_.at("c");
        }
        // as x
        else if(**c_it_ == token_type::CBRAC && **std::next(c_it_) == token_type::OBRAC) {
            nextToken();
            res = var_store_.at("x");
        }
        else     
            throw std::runtime_error("Syntax error");

        nextToken();
        return res;
    }

    bool notC(tokens_cit token_it) const noexcept
    {
        return **token_it == token_type::OBRAC             &&
               **std::next(token_it) != token_type::ADD    &&
               **std::next(token_it) != token_type::SUB    &&
               **std::next(token_it) != token_type::CBRAC;
    }
    bool notX(tokens_cit token_it) const noexcept
    {
        return **token_it == token_type::CBRAC             &&
               **std::next(token_it) != token_type::OBRAC  &&
               **std::next(token_it, 2) != token_type::SUB &&
               **std::next(token_it, 2) != token_type::CBRAC;
    }
    bool emptyExpr(tokens_cit token_it) const noexcept
    {
        return **token_it == token_type::OBRAC &&
               **std::next(token_it) == token_type::CBRAC;
    }

};

inline std::unique_ptr<iToken> newAdd()                     {return std::make_unique<AddToken>();}
inline std::unique_ptr<iToken> newSub()                     {return std::make_unique<SubToken>();}
inline std::unique_ptr<iToken> newOBracket()                {return std::make_unique<ObracketToken>();}
inline std::unique_ptr<iToken> newCBracket()                {return std::make_unique<CbracketToken>();}
inline std::unique_ptr<iToken> newAssign()                  {return std::make_unique<AssignToken>();}
inline std::unique_ptr<iToken> newScolon()                  {return std::make_unique<ScolonToken>();}
inline std::unique_ptr<iToken> newInput()                   {return std::make_unique<InputToken>();}
inline std::unique_ptr<iToken> newOutput()                  {return std::make_unique<OutputToken>();}
inline std::unique_ptr<iToken> newVal(const char* value)    {return std::make_unique<ValueToken>(value);}
inline std::unique_ptr<iToken> newId(const char* id)        {return std::make_unique<IdToken>(id);}

} //namespace my_impl

#endif //RAM_H
