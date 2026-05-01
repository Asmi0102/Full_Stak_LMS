#if defined(_WIN32) || defined(__MINGW32__)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
#endif
#define BOOST_ASIO_HEADER_ONLY
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <sstream>
#include "include/database.h"
#include "services/LibraryService.h"

namespace beast = boost::beast;
namespace http  = beast::http;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;
using json      = nlohmann::json;

json bookToJson(const Book& b){ return {{"id",b.id},{"title",b.title},{"author",b.author},{"genre",b.genre},{"quantity",b.quantity},{"available",b.available},{"issued",b.quantity-b.available}}; }
json memberToJson(const Member& m){ return {{"id",m.id},{"membership_number",m.membership_number},{"name",m.name},{"email",m.email},{"phone",m.phone},{"total_fine",m.total_fine},{"joined_date",m.joined_date}}; }
json txnToJson(const Transaction& t){ return {{"id",t.id},{"member_id",t.member_id},{"book_id",t.book_id},{"member_name",t.member_name},{"book_title",t.book_title},{"issue_date",t.issue_date},{"due_date",t.due_date},{"return_date",t.return_date},{"status",t.status},{"fine",t.fine},{"staff_id",t.staff_id},{"staff_name",t.staff_name}}; }
json staffToJson(const StaffMember& s){ return {{"id",s.id},{"username",s.username},{"name",s.name},{"email",s.email},{"role",s.role},{"created_date",s.created_date},{"first_login",s.first_login}}; }
json ok(const std::string& msg, json data=nullptr){ json r={{"success",true},{"message",msg}}; if(!data.is_null())r["data"]=data; return r; }
json err(const std::string& msg){ return {{"success",false},{"message",msg}}; }

class RequestHandler {
public:
    explicit RequestHandler(LibraryService& svc):svc_(svc){}

    http::response<http::string_body> handle(const http::request<http::string_body>& req){
        http::response<http::string_body> res;
        res.version(req.version()); res.keep_alive(req.keep_alive());
        res.set(http::field::server,"LibraryAPI/1.0");
        res.set(http::field::content_type,"application/json");
        res.set(http::field::access_control_allow_origin,"*");
        res.set(http::field::access_control_allow_methods,"GET, POST, DELETE, OPTIONS");
        res.set(http::field::access_control_allow_headers,"Content-Type");
        if(req.method()==http::verb::options){res.result(http::status::no_content);res.prepare_payload();return res;}

        std::string target=std::string(req.target());
        std::string path=target.substr(0,target.find('?'));
        std::string query=(target.find('?')!=std::string::npos)?target.substr(target.find('?')+1):"";
        json body; try{if(!req.body().empty())body=json::parse(req.body());}catch(...){body=json::object();}
        json result;

        // Books
        if(req.method()==http::verb::get&&path=="/books/all"){auto v=svc_.getAllBooks();json a=json::array();for(auto&b:v)a.push_back(bookToJson(b));result=ok("Books retrieved",a);}
        else if(req.method()==http::verb::get&&path=="/books/available"){auto v=svc_.getAvailableBooks();json a=json::array();for(auto&b:v)a.push_back(bookToJson(b));result=ok("Available",a);}
        else if(req.method()==http::verb::get&&path=="/books/search"){auto q=gp(query,"q");if(q.empty()){result=err("q required");}else{auto v=svc_.searchBooks(q);json a=json::array();for(auto&b:v)a.push_back(bookToJson(b));result=ok(std::to_string(v.size())+" found",a);}}
        else if(req.method()==http::verb::post&&path=="/books/add"){try{auto r=svc_.addBook(body.value("title",""),body.value("author",""),body.value("genre","General"),body.value("quantity",0));result={{"success",r["success"]=="true"},{"message",r["message"]}};if(r["success"]=="true")result["book_id"]=std::stoi(r["id"]);}catch(...){result=err("Invalid body");}}
        else if(req.method()==http::verb::delete_&&sw(path,"/books/remove/")){auto r=svc_.removeBook(xi(path,"/books/remove/"));result={{"success",r["success"]=="true"},{"message",r["message"]}};}
        // Members
        else if(req.method()==http::verb::get&&path=="/members/all"){auto v=svc_.getAllMembers();json a=json::array();for(auto&m:v)a.push_back(memberToJson(m));result=ok("Members",a);}
        else if(req.method()==http::verb::post&&path=="/members/add"){try{auto r=svc_.addMember(body.value("name",""),body.value("email",""),body.value("phone",""));result={{"success",r["success"]=="true"},{"message",r["message"]}};if(r["success"]=="true"){result["member_id"]=std::stoi(r["id"]);result["membership_number"]=r["membership_number"];}}catch(...){result=err("Invalid body");}}
        else if(req.method()==http::verb::delete_&&sw(path,"/members/remove/")){auto r=svc_.removeMember(xi(path,"/members/remove/"));result={{"success",r["success"]=="true"},{"message",r["message"]}};}
        // Transactions
        else if(req.method()==http::verb::post&&path=="/issue"){try{int sid=body.value("staff_id",0);auto r=svc_.issueBook(body.value("member_id",0),body.value("book_id",0),body.value("due_date",""),sid);result={{"success",r["success"]=="true"},{"message",r["message"]}};if(r["success"]=="true"){result["transaction_id"]=std::stoi(r["transaction_id"]);result["issue_date"]=r["issue_date"];result["due_date"]=r["due_date"];result["member"]=r["member"];result["book"]=r["book"];}}catch(...){result=err("Invalid body");}}
        else if(req.method()==http::verb::post&&path=="/return"){try{int sid=body.value("staff_id",0);auto r=svc_.returnBook(body.value("transaction_id",0),sid);result={{"success",r["success"]=="true"},{"message",r["message"]}};if(r["success"]=="true"){result["return_date"]=r["return_date"];result["overdue_days"]=std::stoi(r["overdue_days"]);result["fine"]=std::stod(r["fine"]);result["book"]=r["book"];result["member"]=r["member"];}}catch(...){result=err("Invalid body");}}
        else if(req.method()==http::verb::get&&path=="/transactions"){auto v=svc_.getAllTransactions();json a=json::array();for(auto&t:v)a.push_back(txnToJson(t));result=ok("Transactions",a);}
        else if(req.method()==http::verb::get&&path=="/transactions/active"){auto v=svc_.getActiveTransactions();json a=json::array();for(auto&t:v)a.push_back(txnToJson(t));result=ok("Active",a);}
        // Fines & Settings
        else if(req.method()==http::verb::post&&sw(path,"/fine/clear/")){auto r=svc_.clearFine(xi(path,"/fine/clear/"));result={{"success",r["success"]=="true"},{"message",r["message"]}};}
        else if(req.method()==http::verb::get&&path=="/settings"){result=ok("Settings",{{"fine_rate_per_day",svc_.getFineRate()},{"allowed_days",svc_.getAllowedDays()}});}
        else if(req.method()==http::verb::post&&path=="/settings/fine-rate"){try{auto r=svc_.setFineRate(body.value("fine_rate_per_day",-1.0));result={{"success",r["success"]=="true"},{"message",r["message"]}};}catch(...){result=err("Invalid body");}}
        else if(req.method()==http::verb::post&&path=="/settings/allowed-days"){try{auto r=svc_.setAllowedDays(body.value("allowed_days",-1));result={{"success",r["success"]=="true"},{"message",r["message"]}};}catch(...){result=err("Invalid body");}}
        // Staff
        else if(req.method()==http::verb::get&&path=="/staff/all"){auto v=svc_.getAllStaff();json a=json::array();for(auto&s:v)a.push_back(staffToJson(s));result=ok("Staff",a);}
        else if(req.method()==http::verb::post&&path=="/staff/add"){try{auto r=svc_.addStaff(body.value("username",""),body.value("password",""),body.value("name",""),body.value("email",""),body.value("role","librarian"));result={{"success",r["success"]=="true"},{"message",r["message"]}};if(r["success"]=="true")result["username"]=r["username"];}catch(...){result=err("Invalid body");}}
        else if(req.method()==http::verb::delete_&&sw(path,"/staff/remove/")){auto r=svc_.removeStaff(xi(path,"/staff/remove/"));result={{"success",r["success"]=="true"},{"message",r["message"]}};}
        else if(req.method()==http::verb::post&&path=="/staff/login"){try{auto r=svc_.validateStaffLogin(body.value("username",""),body.value("password",""));result={{"success",r["success"]=="true"},{"message",r["message"]}};if(r["success"]=="true"){result["name"]=r["name"];result["role"]=r["role"];result["username"]=r["username"];result["email"]=r["email"];result["id"]=std::stoi(r["id"]);result["first_login"]=r["first_login"]=="1";}}catch(...){result=err("Invalid body");}}
        else if(req.method()==http::verb::post&&path=="/staff/update-profile"){try{int sid=body.value("staff_id",0);auto r=svc_.updateStaffProfile(sid,body.value("email",""),body.value("new_password",""));result={{"success",r["success"]=="true"},{"message",r["message"]}};}catch(...){result=err("Invalid body");}}
        else if(req.method()==http::verb::post&&sw(path,"/staff/clear-first-login/")){auto r=svc_.clearFirstLogin(xi(path,"/staff/clear-first-login/"));result={{"success",r["success"]=="true"},{"message",r["message"]}};}
        // Admin profile (persistent)
        else if(req.method()==http::verb::get&&path=="/admin/profile"){auto r=svc_.getAdminProfile();result={{"success",true},{"name",r["name"]},{"email",r["email"]}};}
        else if(req.method()==http::verb::post&&path=="/admin/update-username"){try{auto r=svc_.updateAdminUsername(body.value("new_username",""),body.value("current_password",""));result={{"success",r["success"]=="true"},{"message",r["message"]}};if(r["success"]=="true")result["username"]=r["username"];}catch(...){result=err("Invalid body");}}
        else if(req.method()==http::verb::post&&path=="/admin/update-profile"){try{auto r=svc_.updateAdminProfile(body.value("name",""),body.value("email",""),body.value("new_password",""),body.value("current_password",""));result={{"success",r["success"]=="true"},{"message",r["message"]}};if(r["success"]=="true"){result["name"]=r["name"];result["email"]=r["email"];}}catch(...){result=err("Invalid body");}}
        else{result=err("Not found: "+path);res.result(http::status::not_found);res.body()=result.dump(2);res.prepare_payload();return res;}

        bool s=result.value("success",false);
        res.result(s?http::status::ok:http::status::bad_request);
        res.body()=result.dump(2);res.prepare_payload();return res;
    }
private:
    LibraryService& svc_;
    bool sw(const std::string& s,const std::string& p){return s.rfind(p,0)==0;}
    int  xi(const std::string& p,const std::string& pfx){try{return std::stoi(p.substr(pfx.size()));}catch(...){return -1;}}
    std::string gp(const std::string& q,const std::string& key){
        std::istringstream ss(q);std::string tok;
        while(std::getline(ss,tok,'&')){auto eq=tok.find('=');if(eq!=std::string::npos&&tok.substr(0,eq)==key){std::string v=tok.substr(eq+1);for(auto& c:v)if(c=='+')c=' ';return v;}}
        return "";
    }
};

void handleSession(tcp::socket socket, LibraryService& svc){
    beast::flat_buffer buf; RequestHandler handler(svc);
    try{
        while(true){
            http::request<http::string_body> req; beast::error_code ec;
            http::read(socket,buf,req,ec);
            if(ec==http::error::end_of_stream)break;
            if(ec){std::cerr<<"[Net] "<<ec.message()<<"\n";break;}
            std::cout<<"["<<req.method_string()<<"] "<<req.target()<<"\n";
            auto res=handler.handle(req); http::write(socket,res,ec);
            if(ec){std::cerr<<"[Net] Write: "<<ec.message()<<"\n";break;}
            if(!res.keep_alive())break;
        }
        beast::error_code ec; socket.shutdown(tcp::socket::shutdown_send,ec);
    }catch(std::exception& e){std::cerr<<"[Session] "<<e.what()<<"\n";}
}

int main(){
    const std::string DB_PATH="library.db";
    std::cout<<"\n  ==========================================\n  AE Library Management System\n  C++17 | Boost.Beast | SQLite3\n  ==========================================\n\n";
    try{
        Database db(DB_PATH); db.initializeTables(); LibraryService svc(db);
        net::io_context ioc; unsigned short actual_port=8080;
        tcp::acceptor acceptor(ioc); beast::error_code ec;
        for(unsigned short p:{8080,8081,8082,8083}){
            acceptor.open(tcp::v4(),ec);if(ec)continue;
            acceptor.set_option(net::socket_base::reuse_address(true),ec);
            acceptor.bind(tcp::endpoint(net::ip::make_address("0.0.0.0"),p),ec);
            if(!ec)acceptor.listen(net::socket_base::max_listen_connections,ec);
            if(!ec){actual_port=p;break;} acceptor.close();
        }
        if(ec)throw std::runtime_error("Could not bind: "+ec.message());
        std::cout<<"  [OK] Server on http://localhost:"<<actual_port<<"\n  [OK] Database: "<<DB_PATH<<"\n  [OK] Ctrl+C to stop.\n\n";
        while(true){tcp::socket socket(ioc);acceptor.accept(socket);std::thread([s=std::move(socket),&svc]()mutable{handleSession(std::move(s),svc);}).detach();}
    }catch(std::exception& e){std::cerr<<"\n[FATAL] "<<e.what()<<"\n";return 1;}
    return 0;
}
