#include <iostream>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <unistd.h>
#include "json.hpp" 
#include "mqtt/client.h" 
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>

#define QOS 1
#define BROKER_ADDRESS "tcp://localhost:1883"
#define GRAPHITE_HOST "127.0.0.1"//"graphite"
#define GRAPHITE_PORT 2003

std::mutex mtx;

    // Variáveis que precisamos alterar futuramente

    struct Sensors {
        std::string sensor_id;
        std::string data_type;
        int data_interval;
        std::string timestamp[2];
        int value[2];
    };

    std::vector<Sensors> sensors;

    struct Machines
    {
        std::string machine_id;
        std::vector<Sensors> sensors_machine;
    };

std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}


void post_metric(const std::string& machine_id, const std::string& sensor_id, const std::string& timestamp_str, const int value) {
    // Create a Graphite message.
   // graphite::Message message(machine_id + "." + sensor_id, value, timestamp_seconds);

    // Send the Graphite message.
    //graphite::Connection connection(GRAPHITE_HOST, GRAPHITE_PORT);
    //connection.send(machine_id + "." + sensor_id, value, timestamp_str);

    int novo_socket = socket(AF_INET, SOCK_STREAM, 0); //criando uma comunicação sincrona

    // Retorna erro quando nao criar o socket
    if (novo_socket == -1)
    {
        std::cerr << "Error: Failed to create socket" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(GRAPHITE_PORT);
    int address_result = inet_pton(AF_INET, GRAPHITE_HOST, &address.sin_addr);
    //int address_result = inet_aton(AF_INET, GRAPHITE_HOST, &address.sin_addr);
    if (address_result <=0)
    {
        // Erro ao associar o socket
        std::cerr << "Error: Failed to find address" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }
    

    /*int bind_result = bind(novo_socket, (struct sockaddr *)&address, sizeof(address));
    if (bind_result == -1)
    {
        // Erro ao associar o socket
        std::cerr << "Error: Failed to bind socket" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }

    int listen_result = listen(novo_socket, 5);
    if (listen_result == -1)
    {
        // Erro ao colocar o socket no modo de escuta
        std::cerr << "Error: Failed to listen socket" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }*/

    int connect_result = connect(novo_socket, (struct sockaddr *)&address, sizeof(address));
    if (connect_result == -1)
    {
        // Erro ao conectar o socket
        std::cerr << "Error: Failed to connect socket" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }

    /*int accept_result = accept(novo_socket, (struct sockaddr *)&address, &address_len);
    if (accept_result == -1) 
    {
        // Erro ao aceitar a conexão
        std::cerr << "Error: Failed to accept socket" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }*/
    

    std::string post_metric = machine_id + "." + sensor_id;
    std::stringstream metrica;
    metrica << "  " << value << " " << string_to_time_t(timestamp_str);
    

    ssize_t send_result = send(novo_socket,
                               post_metric.c_str(), // Ponteiro para a string de dados métricos.
                               post_metric.size(), // Tamanho da string de dados métricos.
                               0);
    if (send_result == -1) 
    {
        // Erro ao aceitar a conexão
        std::cerr << "Error: Failed to send data" << std::endl;
        close(novo_socket);
        exit(EXIT_FAILURE);
    }

    close(novo_socket);
}

// Código professor
std::vector<std::string> split(const std::string &str, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

void connect_to_mqtt_broker(mqtt::async_client& client, const std::string& broker_address, const std::string& client_id) {
    // Crie uma nova conexão MQTT.
    mqtt::connect_options connOpts;
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    // Tente conectar ao broker.
    try {
        client.connect(connOpts);
    } catch (mqtt::exception& e) {
        // Erro de conexão.
        std::cerr << "Error: Failed to connect" << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }

    // Assine o tópico /sensors/#.
    client.subscribe("/sensors/#", QOS);
}

void send_inactivity_alarms(const std::string& machine_id/*const std::chrono::seconds& inactivity_timeout*/) {
    // Armazene o timestamp da última leitura recebida para cada sensor.
    /*    std::map<std::string, std::chrono::time_point<std::chrono::system_clock>> last_reading_timestamps;

        
        // Verifique se o tempo limite de inatividade foi ultrapassado para qualquer sensor.
        for (const auto& sensor : sensors) {
            // Obtenha o timestamp da última leitura recebida.
            auto last_reading_timestamp = last_reading_timestamps[sensor.sensor_id];

            // Verifique se o tempo limite de inatividade foi ultrapassado.
            if (std::chrono::system_clock::now() - last_reading_timestamp > inactivity_timeout) {
                // Envie um alarme para o Graphite.
                post_metric(machine_id, "alarms.inactive", sensor.sensor_id, 1);
            }
        }
    */

    while(1)
    {
        for (const auto& sensor : sensors) {        
            mtx.lock();

            int last_read = string_to_miliseconds(sensor.timestamp[1]);
            int time_now = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now()).time_since_epoch().count();

            // Verifique se o tempo limite de inatividade foi ultrapassado.
            if (time_now - last_read > sensor.data_interval*10) {
                // Envie um alarme para o Graphite.
                post_metric(machine_id, sensor.sensor_id, "alarms.inactive", 1);
            }

            mtx.unlock();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }     
}

void data_slew_rate(const std::string& machine_id, const std::string& sensor_id) {

    // Define o sensor que será utilizado
    int sens;
    if (sensor_id == sensors[0].sensor_id) sens = 0;
    else sens = 1;

    mtx.lock();
    std::string timestamp_old = sensors[sens].timestamp[0], timestamp_new = sensors[sens].timestamp[1];
    int value_old = sensors[sens].value[0], value_new = sensors[sens].value[1];

    int interval = string_to_miliseconds(timestamp_new) - string_to_miliseconds(timestamp_old);
    int slew_rate = (value_new - value_old) / interval;

    post_metric(machine_id, sensors[sens].sensor_id, "Slew rate data: ", slew_rate);
    mtx.unlock();
}

void post_data_CPU(const std::string& machine_id){
    
    while (1){


        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void post_data_memoria(const std::string& machine_id){

    while (1){

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char* argv[]) {

    //Iniciando vetor de sensores
    sensors.push_back({"CPU_Percentage_Used", "percent", 30, {0, 0}, {0, 0}});
    sensors.push_back({"Memory_Percentage_Used", "percent", 25, {0, 0}, {0, 0}});

    std::string clientId = "clientId";
    mqtt::async_client client(BROKER_ADDRESS, clientId);


    // Create an MQTT callback.
    class callback : public virtual mqtt::callback {
    public:

        void message_arrived(mqtt::const_message_ptr msg) override {
            auto j = nlohmann::json::parse(msg->get_payload());

            std::string topic = msg->get_topic();
            auto topic_parts = split(topic, '/');
            std::string machine_id = topic_parts[2];
            std::string sensor_id = topic_parts[3];


            std::string timestamp = j["timestamp"];
            int value = j["value"];
            post_metric(machine_id, sensor_id, timestamp, value);

            // Atualiza os valores dos sensores
            if(sensor_id == "001") {
                sensors[0].timestamp[0] = sensors[0].timestamp[1];
                sensors[0].timestamp[1] = timestamp;
                sensors[0].value[0] = sensors[0].value[1];
                sensors[0].value[1] = value;
            }
            else{
                sensors[1].timestamp[0] = sensors[1].timestamp[1];
                sensors[1].timestamp[1] = timestamp;
                sensors[1].value[0] = sensors[1].value[1];
                sensors[1].value[1] = value;
            }
 
            // Processamento de alarmes
            std::thread inactive_alarm_process(send_inactivity_alarms, machine_id);
            

            // Processamento dos dados
            //std::thread cpu_process(post_data_cpu, machine_id);
            //std::thread memory_process(post_data_memoria, machine_id);
            std::thread slew_rate_process(data_slew_rate, machine_id, sensor_id);
        }
    };

    callback cb;
    client.set_callback(cb);

    // Connect to the MQTT broker.
    mqtt::connect_options connOpts;
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    try {
        client.connect(connOpts);
        client.subscribe("/sensors/#", QOS);
    } catch (mqtt::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Iniciando processamentos em paralelo

        
        //std::thread slew_rate_process(data_slew_rate, machine_id, sensor_id); //passar isso sempre q vier um novo dado? ele n pode rodar em loop do jeito q eu fiz lá em cima

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return EXIT_SUCCESS;
}