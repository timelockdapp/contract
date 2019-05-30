#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/transaction.hpp> // 
using namespace eosio;
using namespace std;

/*
timelockdapp.cpp 
[eosio-cpp version 1.3.2]

(2019/may/25) - Beginn V0.1
*/

const std::string   version   = "V0.1";
 

CONTRACT timelockdapp : public eosio::contract {

  public:
      using contract::contract;
      
      // ---
      // Constants
      //
      const uint32_t  dbgval   = 99984;      
      const name team_address1 = name("timelockteam");   
      const name team_address2 = name("stakeeossafe");   
       
      struct paramstruct
      {
      std::string param;    
      };

 
      struct transfer_args
      {
      name from;
      name to;
      asset quantity;
      std::string memo;
      };    

      
      // ---
      // struct global
      //
      struct global
      {
      uint64_t id; 
      uint64_t accounts;    // Number of accounts

      uint64_t funds_open;
      uint64_t funds_closed;
      
      auto primary_key() const { return id; }
      EOSLIB_SERIALIZE(global, (id)(accounts)(funds_open)(funds_closed) )
      }; // struct global            
      typedef eosio::multi_index< name("global"), global> globals;
           
     
      // ---
      // Struct for single deposit
      //
      struct depstruct
      {
      uint64_t      id;       // increment number      
      
      name          holder;
      uint64_t      holderid; // holder id       
      
      name          depositidhr; 
      uint64_t      depositid;
      
      uint64_t      amount;
      uint64_t      fee;
    
      bool          withdrawn;
      uint64_t      timewithdraw;  
             
      auto primary_key() const { return id; }   
      uint64_t by_holder() const { return holderid; }          
      uint64_t by_depositid() const { return depositid; }   
          
      EOSLIB_SERIALIZE(depstruct,(id)(holder)(holderid)(depositidhr)(depositid)(amount)(fee)(withdrawn)(timewithdraw) )    
      }; // depstruct
      
      typedef eosio::multi_index<name("depstruct"), depstruct ,    indexed_by<name("byholder"), const_mem_fun<depstruct, uint64_t, &depstruct::by_holder>>  ,  indexed_by<name("bydepositid"), const_mem_fun<depstruct, uint64_t, &depstruct::by_depositid>>         > depstructs;

       
      
      // ---
      // Status - minimal state
      //
      [[eosio::action]]  
      void status() 
      {    
      print(" TIMELOCK VERSION: ",version," ");
      } // status() 
    



 
      inline void splitMemo(std::vector<std::string> &results, std::string memo)
            {
            auto end = memo.cend();
            auto start = memo.cbegin();

            for (auto it = memo.cbegin(); it != end; ++it)
                {
                if (*it == ';')
                   {
                   results.emplace_back(start, it);
                   start = it + 1;
                   }
                }
            if (start != end)
            results.emplace_back(start, end);
            } // splitMemo





   


      // ---
      // send_token - sending token.
      //  
      inline void send_token(name receiver, uint64_t amount, std::string memo) 
      {              
      asset quant = asset(amount, symbol("EOS", 4)  );            
      action(
            permission_level{_self, name("active")},
            name("eosio.token"), name("transfer"),
            std::make_tuple(_self, receiver, quant, memo )
            ).send();
      } // void send_token()


      
 
   
      // ---
      // handle_transfer()
      //
      [[eosio::action]]  
      void handle_transfer()
      {
      auto data = unpack_action_data<transfer_args>();
      
      std::vector<std::string> results;
      splitMemo(results, data.memo);
        
      
      string thesymbol = "";
      int found_symbol = 0;
      if ( data.quantity.symbol == symbol("EOS" , 4) ) 
         {
         thesymbol = "EOS";
         found_symbol = 1;
         }
                          
      eosio_assert( found_symbol == 1  , " Deposit hast to be EOS! ");
         
         

 
      if (results[0] == "deposit")
         { 
         // prevention
         eosio_assert( data.to == _self  , "No!");  
         cleanup();
         
         name        d_depositid     = name( results[1] );  //e.g. trarofde5sae
         name        d_holder        = name( results[2] );  //e.g. myeosaccount
         
         uint64_t    d_amount        = std::stoull( results[3] );
         uint64_t    d_ts            = std::stoull( results[4] );
           
         eosio_assert(  d_amount > 0  , " Amount must not be zero! ");
         eosio_assert(  d_amount >= 1000  , " Minimum amount 0.1 EOS! ");
           
         eosio_assert(  d_ts < 253118303999  , " Timestamp to large ");
         eosio_assert(  d_ts >  now()  , " Timestamp is in the past ");
           
         // ---  
         // Fee calculation... 
         //
         uint64_t    d_fee = d_amount * 0.01;
         eosio_assert(  (data.quantity.amount)  >= (d_amount+d_fee)   , " Amount to low! ");
         
         
         
         
         //
         // Get globals
         //
         int id = 0;
         globals myglobals(_self, _self.value);
         auto iterator_globals = myglobals.find(id);

         
         uint64_t g_accounts   = iterator_globals->accounts;
         uint64_t funds_open   = iterator_globals->funds_open;
         uint64_t funds_closed = iterator_globals->funds_closed;
         
      
         
         // --- 
         // is d_depositid still registered?
         //
         depstructs mydepstructs(_self, _self.value);
                                      
         auto zip_index = mydepstructs.get_index<name("bydepositid")>();     
         auto iterator_dep = zip_index.find( d_depositid.value );
      
         uint32_t counter = 0;
  
         if ( iterator_dep->depositid == d_depositid.value )
            {            
            int ERROR = 1;
            eosio_assert( ERROR == 0 , " DEPOSIT-ID STILL EXIST! ");              
            } else 
              {
              // Create new entry
              g_accounts++;
              
              funds_open = funds_open + d_amount;
                              
              mydepstructs.emplace(_self, [&](auto&  tupel) 
                {                
                tupel.id             = g_accounts;                
                tupel.holder         = d_holder;
                tupel.holderid       = d_holder.value;                
                tupel.depositidhr    = d_depositid;                
                tupel.depositid      = d_depositid.value;                
                tupel.amount         = d_amount;
                tupel.fee            = d_fee;                                
                tupel.withdrawn      = false;                                
                tupel.timewithdraw   = d_ts;
                }); 
                               
              
              // ---
              // Update globals
              //                                
              myglobals.modify(iterator_globals, _self, [&](auto& global) 
                   {                                                          
                   global.accounts           = g_accounts;     
                   global.funds_open         = funds_open;     
                   global.funds_closed       = funds_closed;                                                                                                
                   });                      
              }
            
 
        
         } // if (results[0] == "deposit")

    
          
       
      } // handle_transfer
    
    
    
      
 
 
      
      
      // ---
      // admin - superuser functions.
      //
      [[eosio::action]]  
      void admin() 
      {
      print(" ADMIN ");     

                                               
      eosio_assert( has_auth( get_self() ), " Missing required authority of Admin");
    
      auto data = unpack_action_data<timelockdapp::paramstruct>();
          
      std::vector<std::string> results;
      splitMemo(results, data.param);
      
      
      if (results[0] == "init")
         {         
         
         // ---
         // Globals
         //
         int id = 0;
         globals myglobals( _self , _self.value );         
         auto iterator = myglobals.find(id);
    
         if ( iterator != myglobals.end() )
            {
            print(" Globals exists! ");
            } else
              {
              print(" Globals will be created! ");
              myglobals.emplace(_self, [&](auto& global) 
                 {
                 global.id              = id;
                 global.accounts        = 0;     
                 global.funds_open = 0;
                 global.funds_closed = 0;                         
                 });
             } // else
                             
         } // init
         
         
         
         
      if (results[0] == "resetglobals")
         {  
         print("Reset globals");
         
         globals myglobals( _self , _self.value ); 
      
         
         for (auto itr = myglobals.begin(); itr != myglobals.end();) 
             {
             itr = myglobals.erase(itr);
             }
             
         } // resetglobals




      if (results[0] == "resetfunds")
         {  
         print("Reset funds");         
         depstructs mydepstructs( _self , _self.value ); 
      
         
         for (auto itr = mydepstructs.begin(); itr != mydepstructs.end();) 
             {
             itr = mydepstructs.erase(itr);
             }
             
         } // resetfunds
         
         
      if (results[0] == "cleanup")
         {  
         cleanup();                      
         } // cleanup
                  
         
         
         
    print(" ADMIN_fin ");   
  
    } // void admin()
       
   
      // ---
      // clean_games - Clean-up after 24 hours.
      //  
      void cleanup()
      {
      print(" Clean All... ");
      
      uint32_t tsnow = now();
      uint32_t seconds = (60*60*24);
                         
      depstructs mydepstructs( _self , _self.value );  
      auto iterator_mydepstructs = mydepstructs.find(0);   

      for (auto itr = mydepstructs.begin(); itr != mydepstructs.end(); )  
             {
             print(" ID:",itr->id," ");
             
        
             // Condition                  
             if ( 
                (itr->timewithdraw > 0) 
                && (itr->withdrawn == 1)   
                &&    
                      ( 
                        ( (itr->timewithdraw+seconds)   < tsnow )  
                        
                      )
                )
                {
                itr = mydepstructs.erase(itr);
                }
                else
                    {
                    itr++ ;
                    }
                    
             } // for ...
      
            
            
      } // cleanup
      
       
  
      
      
      
       
      // ---
      // withdraw  
      //
      [[eosio::action]]  
      void withdraw() 
      {
      print(" DO withdraw - ");
      cleanup();
       
      //
      // Get globals
      //
      int id = 0;
      globals myglobals(_self, _self.value);
      auto iterator_globals = myglobals.find(id);         
     
      uint64_t funds_open   = iterator_globals->funds_open;
      uint64_t funds_closed = iterator_globals->funds_closed;
               
      auto data = unpack_action_data<timelockdapp::paramstruct>();
   
      std::vector<std::string> results;
      splitMemo(results, data.param);      
      name        d_depositid     = name( results[0] );  //e.g. trarofde5sae
      
      
      // ---
      // does depositid exist?
      //
      depstructs mydepstructs(_self, _self.value);
      
      auto zip_index = mydepstructs.get_index<name("bydepositid")>();   
      auto iterator_dep = zip_index.find( d_depositid.value );
      
  
      if (
         iterator_dep->depositid == d_depositid.value             
         )
         {                             
         eosio_assert( has_auth( iterator_dep->holder ), " missing required authority ");
                               
         // Timelock
         eosio_assert( now() >= iterator_dep->timewithdraw     , " The time has not come! " );
                         
         // Disallow double withdraw  
         eosio_assert( iterator_dep->withdrawn == false   , " Withdraw already has done! " );
             
         //
         // 1) Send EOS to holder
         //        
         std::string str_d_depositid =  name{d_depositid}.to_string();                  
         string memo = "Timelockdapp - finaly your EOS are unlocked now! Deposit-ID: " +str_d_depositid+ "";
         send_token( iterator_dep->holder, iterator_dep->amount,  memo) ;    

         //
         // 2) Send Fee
         //         
         uint64_t fee   = iterator_dep->fee * 0.5;
         memo = "Timelockdapp - Maintenance fee";
         send_token( team_address1, fee,  memo) ;    

         memo = "Timelockdapp - Maintenance fee";
         send_token( team_address2, fee,  memo) ;    
         
 
         //
         // 3) Update from 0 to 1
         //              
         auto iterator_dep2 = mydepstructs.find( iterator_dep->id );
        
         if ( iterator_dep2 != mydepstructs.end() )
            {            
            mydepstructs.modify(iterator_dep2, _self, [&](auto& tupel) 
                 {                                         
                 tupel.withdrawn   = true;                                        
                 });
               
            funds_open   = funds_open   - iterator_dep->amount;    
            funds_closed = funds_closed + iterator_dep->amount;    
                  
            // ---
            // Update globals / increase accounts-number
            //                                
            myglobals.modify(iterator_globals, _self, [&](auto& global) 
                   {                                                                                
                   global.funds_open       = funds_open;     
                   global.funds_closed     = funds_closed;                                                                                                
                   });   
                  
            } else
              {
              int ERROR = 1;
              eosio_assert( ERROR == 0 , " DATA NOT FOUND! ");
              }
          
            } else 
              {
              int ERROR = 1;
              eosio_assert( ERROR == 0 , " DEPOSIT-ID NOT FOUND! ");
              }
              
      } // withdraw()     
    
    
      
      
      
}; // CONTRACT timelockdapp       




extern "C"
{

void apply(uint64_t receiver, uint64_t code, uint64_t action) 
{

    if (
       code   == name("eosio.token").value   &&
       action == name("transfer").value
       )
       {       
       execute_action(name(receiver), name(code), &timelockdapp::handle_transfer);  
       }

    if (action == name("status").value)
       {
       execute_action(name(receiver), name(code), &timelockdapp::status);  
       }
       
    if (action == name("withdraw").value)
       {
       execute_action(name(receiver), name(code), &timelockdapp::withdraw);  
       }       

    if (action == name("admin").value)
       {
       execute_action(name(receiver), name(code), &timelockdapp::admin);  
       }
 
} // apply
    
    
    
       
} // extern "C"      

