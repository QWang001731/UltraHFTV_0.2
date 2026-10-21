#include<cstdint>
#include<cstring>
#include<string>
#include<unordered_map>
#include<map>
#include "read_big_endian_number.h"
#include<iostream>

#pragma once

enum Side
{
    Buy,
    Sell
};

struct Order {
    uint64_t _order_id;
    uint32_t _shares;
    uint32_t _price;
    Side _side;
    uint32_t _stock_id;
};

struct Price_level
{
    uint32_t _shares=0;
    uint32_t _price=0;
};


class Level2_Order_Book
{

private:

    uint32_t _stock_id;
    std::string _symbol;
    std::unordered_map<uint64_t,Order>_orders;
    std::map<uint32_t,Price_level> _asks;
    std::map<uint32_t,Price_level,std::greater<uint32_t>> _bids;
    

public:
    Level2_Order_Book(std::string symbol, uint32_t stock_id);
    ~Level2_Order_Book();
    bool Add_order(const uint8_t* p);
    bool Delete_order(const uint8_t* p);
    bool Execute_order(const uint8_t* p);
    bool Cancel_order(const uint8_t* p);
    bool Replace_order(const uint8_t* p);
    bool Validate() const;

};


Level2_Order_Book::Level2_Order_Book(std::string symbol, uint32_t stock_id)
{
    _symbol =std::move(symbol);
    _stock_id = stock_id;
}

bool Level2_Order_Book::Add_order(const uint8_t* p)
{
    uint64_t order_id = read_be::read_be64(p + 11);
    uint32_t shares = read_be::read_be32(p + 20);
    uint32_t price = read_be::read_be32(p + 32);

    Side side = p[19] == 'B' ? Side::Buy : Side::Sell;
    
    _orders[order_id] =  Order{
    order_id,
    shares,
    price,
    side,
    _stock_id
    };


    if(side == Side::Buy)
    {
        _bids[price]._shares += shares;

    }
    else
    {
        _asks[price]._shares += shares;

    }

    return true;
}

bool Level2_Order_Book::Delete_order(const uint8_t* p)
{

    uint64_t order_id = read_be::read_be64(p + 11);
    if(_orders.find(order_id) == _orders.end())
    {
        return false;
    }
    else
    {
        Side side = _orders[order_id]._side;
        uint32_t price = _orders[order_id]._price;
        uint32_t shares = _orders[order_id]._shares;
        _orders.erase(order_id);

        if(side == Side::Buy)
        {
            if( _bids[price]._shares <= shares )
            {
                _bids.erase(price);
            }

            else
            {
                _bids[price]._shares -= shares;

            }
        }
        else[[likely]]
        {
            if(_asks[price]._shares <= shares)
            {
                _asks.erase(price);
            }
            else
            {
                _asks[price]._shares -= shares;

            }
        }

        return true;

    }
}

bool Level2_Order_Book::Execute_order(const uint8_t *p)
{
    uint64_t order_id = read_be::read_be64(p + 11);
    
    uint32_t shares = read_be::read_be32(p + 19);
    
    if(_orders.find(order_id) != _orders.end())[[likely]]
    {
        Side side = _orders[order_id]._side;
        uint32_t price = _orders[order_id]._price;
        
        if(_orders[order_id]._shares < shares)
        {
            std::cerr<<"order ID :" <<order_id<<" execueted more shares than stored\n";
            return false;
        }

        _orders[order_id]._shares -= shares;
        
        if(_orders[order_id]._shares == 0)
        {
            _orders.erase(order_id);
        }
        
        if(side == Side::Buy)
        {
            _bids[price]._shares -=shares;

            if(_bids[price]._shares == 0)
                {
                    _bids.erase(price);
                }
        }
        else
        {
            _asks[price]._shares -= shares;
            
            if(_asks[price]._shares == 0)
                {
                    _asks.erase(price);                        
                }
        }
        return true;
    }

    else
    {
        return false;
    }
}

bool Level2_Order_Book:: Cancel_order(const uint8_t* p)
{

    uint64_t order_id = read_be::read_be64(p + 11);
    u_int32_t cancel_shares = read_be::read_be32(p + 19);


    if(_orders.find(order_id) != _orders.end())[[likely]]
    {

        Side side = _orders[order_id]._side;
        uint32_t price = _orders[order_id]._price;

        if(_orders[order_id]._shares < cancel_shares)
        {
            std::cerr<<"order ID :" <<order_id<<" cancled more shares than stored\n";
            return false;
        }

        _orders[order_id]._shares -= cancel_shares;
        
        if(_orders[order_id]._shares == 0)
        {
            _orders.erase(order_id);
        }

        if(side == Side::Buy)
        {

            _bids[price]._shares -= cancel_shares;

            if(_bids[price]._shares == 0)
            {
                _bids.erase(price);
            }

        }

        else
        {
            _asks[price]._shares -= cancel_shares;

            if(_asks[price]._shares == 0)
            {
                _asks.erase(price);

            }

        }

        return true;

    }
    else
    {

        return false;
    }
}


bool Level2_Order_Book::Replace_order(const uint8_t *p)
{

    uint64_t original_order_id = read_be::read_be64(p + 11);
    uint64_t new_order_id = read_be::read_be64(p + 19);
    uint32_t new_shares = read_be::read_be32(p + 27);
    uint32_t new_price = read_be::read_be32(p + 31);
    
    if(_orders.find(original_order_id) != _orders.end())[[likely]]
    {
        Side side = _orders[original_order_id]._side;
        uint32_t original_shares = _orders[original_order_id]._shares;
        uint32_t original_price = _orders[original_order_id]._price;

        _orders.erase(original_order_id);

        _orders[new_order_id] = Order{new_order_id,new_shares,new_price,side,_stock_id};
        
        if(side == Side::Buy)
        {
            _bids[original_price]._shares -= original_shares;
            
            if(_bids[original_price]._shares == 0)
            {
                _bids.erase(original_price);

            }

            _bids[new_price]._shares += new_shares;

        }
        else
        {
            _asks[original_price]._shares -= original_shares;
            if(_asks[original_price]._shares == 0)
            {
                _asks.erase(original_price);
            }

            _asks[new_price]._shares += new_shares;
        }

        return true;
    }

    else
    {
        return false;

    }  

}


bool Level2_Order_Book::Validate() const
{
    std::map<uint32_t, uint64_t> expected_asks;
    std::map<uint32_t, uint64_t, std::greater<uint32_t>> expected_bids;

    for (const auto& [order_id, order] : _orders)
    {
        if (order._shares == 0)
            return false;

        if (order._side == Side::Buy)
            expected_bids[order._price] += order._shares;
        else
            expected_asks[order._price] += order._shares;
    }

    if (expected_bids.size() != _bids.size())
        return false;

    if (expected_asks.size() != _asks.size())
        return false;

    for (const auto& [price, expected_shares] : expected_bids)
    {
        auto it = _bids.find(price);

        if (it == _bids.end())
            return false;

        if (it->second._shares != expected_shares)
            return false;
    }

    for (const auto& [price, expected_shares] : expected_asks)
    {
        auto it = _asks.find(price);

        if (it == _asks.end())
            return false;

        if (it->second._shares != expected_shares)
            return false;
    }

    return true;
}

Level2_Order_Book::~Level2_Order_Book()
{
    
}




