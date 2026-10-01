#include "SimulationController.h"
#include "std_msgs/msg/float64.hpp"
#include <random>
#include "ros_gz_interfaces/msg/contacts.hpp"
#include "trajectory_msgs/msg/joint_trajectory.hpp"
#include "trajectory_msgs/msg/joint_trajectory_point.hpp"
#include <chrono>
#include <thread>
#include <cstdlib>


SimulationController::SimulationController() : rclcpp::Node("simulation_controller"){
    // Inicializar los límites de las articulaciones (en radianes)

    jointLimits = {{-0.5, 0.5},  // jnt_pecho_hombro
                   {-1.0, 1.0},  // jnt_hombro_hombro
                   {-0.5, 0.5},  // jnt_hombro_biceps
                   {-0.5, 0.5},  // jnt_biceps_codo
                   {-1.0, 1.0},  // jnt_codo_antebrazo
                   {-1.0, 1.0},  // jnt_antebrazo_palma
                   {-1.0, 1.0},  // jnt_palma_pulgar_1 (6)
                   {-1.0, 1.0},  // jnt_pulgar_1_2
                   {-1.0, 1.0},  // jnt_pulgar_2_3
                   {-1.0, 1.0},  // jnt_palma_indice_1 (9)
                   {-1.0, 1.0},  // jnt_indice_1_2
                   {-1.0, 1.0},  // jnt_indice_2_3
                   {-1.0, 1.0},  // jnt_palma_cordial_1 (12)
                   {-1.0, 1.0},  // jnt_cordial_1_2
                   {-1.0, 1.0},  // jnt_cordial_2_3
                   {-1.0, 1.0},  // jnt_palma_anular_1 (15)
                   {-1.0, 1.0},  // jnt_anular_1_2
                   {-1.0, 1.0},  // jnt_anular_2_3
                   {-1.0, 1.0},  // jnt_palma_menique_1 (18)
                   {-1.0, 1.0},  // jnt_menique_1_2
                   {-1.0, 1.0}}; // jnt_menique_2_3
                   
    // Inicializar valores por defecto
    jointValues.assign(TOTAL_JOINTS, 0.0);

    // Publicador para trayectoria completa
    jointTrajectoryPub = this->create_publisher<trajectory_msgs::msg::JointTrajectory>("/joint_trajectory_controller/joint_trajectory", 10);
    // Publicador para el controlador de esfuerzo
    jointEffortPub = this->create_publisher<std_msgs::msg::Float64>("/effort_controller/command", 10);



    // Suscriptor para bumper palma
    suscriptorPalma = this ->create_subscription<ros_gz_interfaces::msg::Contacts>
        ("/bumper_states_palma", rclcpp::SensorDataQoS(), std::bind(&SimulationController::deteccionColisionPalma, this, std::placeholders::_1));

    // Suscriptor para bumper antebrazo
    suscriptorAntebrazo= this ->create_subscription<ros_gz_interfaces::msg::Contacts>
        ("/bumper_states_antebrazo", rclcpp::SensorDataQoS(), std::bind(&SimulationController::deteccionColision, this, std::placeholders::_1));
    
    // Suscriptor para dedo pulgar
    suscriptorPulgar= this ->create_subscription<ros_gz_interfaces::msg::Contacts>
        ("/bumper_states_pulgar_3", rclcpp::SensorDataQoS(), std::bind(&SimulationController::deteccionColision, this, std::placeholders::_1));
    // Suscriptor para bumper dedo indice
    suscriptorIndice= this ->create_subscription<ros_gz_interfaces::msg::Contacts>
        ("/bumper_states_indice_3", rclcpp::SensorDataQoS(), std::bind(&SimulationController::deteccionColision, this, std::placeholders::_1));
    // Suscriptor para bumper dedo cordial
    suscriptorCordial= this ->create_subscription<ros_gz_interfaces::msg::Contacts>
        ("/bumper_states_cordial_3", rclcpp::SensorDataQoS(), std::bind(&SimulationController::deteccionColision, this, std::placeholders::_1));
    // Suscriptor para bumper dedo anular
    suscriptorAnular= this ->create_subscription<ros_gz_interfaces::msg::Contacts>
        ("/bumper_states_anular_3", rclcpp::SensorDataQoS(), std::bind(&SimulationController::deteccionColision, this, std::placeholders::_1));
    // Suscriptor para bumper dedo menique
    suscriptorMenique= this ->create_subscription<ros_gz_interfaces::msg::Contacts>
        ("/bumper_states_menique_3", rclcpp::SensorDataQoS(), std::bind(&SimulationController::deteccionColision, this, std::placeholders::_1));
    
    timer_ = this->create_timer(
        std::chrono::milliseconds(2500),
        std::bind(&SimulationController::startTrajectory, this));

    // Romper la soldadura inicial del DetachableJoint sin bloquear el constructor.
    // Wall timer porque necesitamos tiempo real, no sim time.
    magnet_off_timer_ = this->create_wall_timer(
        std::chrono::seconds(3), [this]() {
            magnet_off_timer_->cancel();
            system("gz topic -t /xolobot_arm/magnet_off -m gz.msgs.Empty -p ' ' &");
            RCLCPP_INFO(this->get_logger(), "Soltando lata (magnet_off enviado).");
        });
}
SimulationController::~SimulationController() {}

void SimulationController::deteccionColision(const ros_gz_interfaces::msg::Contacts::SharedPtr msg){
    if(colisionDetectada) return;
    for (const auto & contact : msg->contacts) {
        const std::string & col1 = contact.collision1.name;
        const std::string & col2 = contact.collision2.name;
        if(col1.find("_izq") != std::string::npos || col2.find("_izq") != std::string::npos) {
            RCLCPP_INFO(this->get_logger(), "Choque: %s <--> %s", col1.c_str(), col2.c_str());
        }
        bool toca_robot = (col1.find("palma")   != std::string::npos || col2.find("palma")   != std::string::npos ||
                           col1.find("pulgar")  != std::string::npos || col2.find("pulgar")  != std::string::npos ||
                           col1.find("indice")  != std::string::npos || col2.find("indice")  != std::string::npos ||
                           col1.find("cordial") != std::string::npos || col2.find("cordial") != std::string::npos ||
                           col1.find("anular")  != std::string::npos || col2.find("anular")  != std::string::npos ||
                           col1.find("menique") != std::string::npos || col2.find("menique") != std::string::npos);
        bool toca_lata  = (col1.find("objeto") != std::string::npos || col1.find("coke_can") != std::string::npos ||
                           col2.find("objeto") != std::string::npos || col2.find("coke_can") != std::string::npos);
        if (toca_robot && toca_lata) {
            colisionDetectada = true;
            RCLCPP_WARN(this->get_logger(), "¡Colision real con lata! [%s / %s]", col1.c_str(), col2.c_str());
            RCLCPP_INFO(this->get_logger(), "¡CONTACTO CONFIRMADO CON LA LATA! CERRANDO DEDOS...");
            agarre_objeto();
            if (!temporizadorHombro) {
                temporizadorHombro = this->create_timer(
                    std::chrono::seconds(5), std::bind(&SimulationController::moverHombro, this));
            }
            break;
        }
    }
}

void SimulationController::deteccionColisionPalma(const ros_gz_interfaces::msg::Contacts::SharedPtr msg){
    if(colisionDetectada) return;
    for (const auto & contact : msg->contacts) {
        const std::string & col1 = contact.collision1.name;
        const std::string & col2 = contact.collision2.name;
        if(col1.find("_izq") != std::string::npos || col2.find("_izq") != std::string::npos) {
            RCLCPP_INFO(this->get_logger(), "Choque: %s <--> %s", col1.c_str(), col2.c_str());
        }
        bool toca_robot = (col1.find("palma")   != std::string::npos || col2.find("palma")   != std::string::npos ||
                           col1.find("pulgar")  != std::string::npos || col2.find("pulgar")  != std::string::npos ||
                           col1.find("indice")  != std::string::npos || col2.find("indice")  != std::string::npos ||
                           col1.find("cordial") != std::string::npos || col2.find("cordial") != std::string::npos ||
                           col1.find("anular")  != std::string::npos || col2.find("anular")  != std::string::npos ||
                           col1.find("menique") != std::string::npos || col2.find("menique") != std::string::npos);
        bool toca_lata  = (col1.find("objeto") != std::string::npos || col1.find("coke_can") != std::string::npos ||
                           col2.find("objeto") != std::string::npos || col2.find("coke_can") != std::string::npos);
        if (toca_robot && toca_lata) {
            colisionDetectada = true;
            RCLCPP_WARN(this->get_logger(), "¡Colision real con lata (palma)! [%s / %s]", col1.c_str(), col2.c_str());
            RCLCPP_INFO(this->get_logger(), "¡CONTACTO CONFIRMADO CON LA LATA! CERRANDO DEDOS...");
            agarre_objeto();
            if (!temporizadorHombro) {
                temporizadorHombro = this->create_timer(
                    std::chrono::seconds(5), std::bind(&SimulationController::moverHombro, this));
            }
            break;
        }
    }
}

void SimulationController::startTrajectory(){
    //RCLCPP_INFO(this->get_logger(), "Iniciando simulacion");
    generaAleatorios(); 
}

void SimulationController::moverHombro(){
    if(!colisionDetectada) return;

    temporizadorHombro->cancel();
    levantando = true;
    RCLCPP_INFO(this->get_logger(), "¡Elevando suavemente con la lata!");

    trajectory_msgs::msg::JointTrajectory liftMsg;
    liftMsg.header.stamp = this->now() + rclcpp::Duration::from_seconds(0.5);
    liftMsg.joint_names = {
        "jnt_pecho_hombro", "jnt_hombro_hombro", "jnt_hombro_biceps",
        "jnt_biceps_codo", "jnt_codo_antebrazo", "jnt_antebrazo_palma",
        "jnt_palma_pulgar_1", "jnt_pulgar_1_2", "jnt_pulgar_2_3",
        "jnt_palma_indice_1", "jnt_indice_1_2", "jnt_indice_2_3",
        "jnt_palma_cordial_1", "jnt_cordial_1_2", "jnt_cordial_2_3",
        "jnt_palma_anular_1", "jnt_anular_1_2", "jnt_anular_2_3",
        "jnt_palma_menique_1", "jnt_menique_1_2", "jnt_menique_2_3"
    };

    trajectory_msgs::msg::JointTrajectoryPoint point;
    // Pose de agarre idéntica pero jnt_hombro_biceps elevado para levantar la lata
    point.positions = {
        0.159,    // jnt_pecho_hombro   — sin cambio
        0.0,      // jnt_hombro_hombro  — sin cambio
        -0.5,     // jnt_hombro_biceps  — sube desde -1.2874 (elevación gradual)
        1.5010,   // jnt_biceps_codo    — sin cambio
        -1.068,   // jnt_codo_antebrazo — sin cambio
        -1.1309,  // jnt_antebrazo_palma — sin cambio (mantiene ángulo de garra)
        // Dedos cerrados — idénticos al agarre
        1.5708, 0.2094, 0.5236,    // pulgar
        0.80,   0.6109, 0.6981,    // índice
        1.1345, 0.6109, 0.6109,    // cordial
        1.1345, 0.6109, 0.6109,    // anular
        0.80,   0.6109, 0.6109     // meñique
    };
    point.velocities.assign(TOTAL_JOINTS, 0.0);
    point.accelerations.assign(TOTAL_JOINTS, 0.0);
    point.time_from_start.sec = 5;  // 5 segundos — elevación lenta para no romper imán

    liftMsg.points.push_back(point);
    jointTrajectoryPub->publish(liftMsg);
}

void SimulationController::agarre_objeto(){
    system("gz topic -t /xolobot_arm/magnet_on -m gz.msgs.Empty -p ' ' &");
    RCLCPP_INFO(this->get_logger(), "¡Imán activado via gz topic!");
}

void SimulationController::generaAleatorios(){
    static int contador_inicio = 0;
    static bool pose_acercamiento_enviada = false;

    // 1. Esperar que Gazebo esté escuchando (reducido a 4 ticks = 10s)
    if (contador_inicio < 4) {
        contador_inicio++;
        return;
    }

    // 2. Silencio total mientras moverHombro ejecuta la elevación
    if (levantando) return;

    // 3. Candado lógico: guardar silencio si la orden ya se envió
    if (!colisionDetectada && pose_acercamiento_enviada) {
        return;
    }

    // 3. Marcar envío y avisar en la terminal
    if (!colisionDetectada) {
        pose_acercamiento_enviada = true;
        RCLCPP_INFO(this->get_logger(), "¡Conexión lista! Enviando pose maestra a la lata...");
    }

    trajectory_msgs::msg::JointTrajectory jointTrajectoryMsg;
    jointTrajectoryMsg.header.stamp = this->now() + rclcpp::Duration::from_seconds(0.5);
    jointTrajectoryMsg.joint_names = {
        "jnt_pecho_hombro", "jnt_hombro_hombro", "jnt_hombro_biceps",
        "jnt_biceps_codo", "jnt_codo_antebrazo", "jnt_antebrazo_palma",
        "jnt_palma_pulgar_1", "jnt_pulgar_1_2", "jnt_pulgar_2_3",
        "jnt_palma_indice_1", "jnt_indice_1_2", "jnt_indice_2_3",
        "jnt_palma_cordial_1", "jnt_cordial_1_2", "jnt_cordial_2_3",
        "jnt_palma_anular_1", "jnt_anular_1_2", "jnt_anular_2_3",
        "jnt_palma_menique_1", "jnt_menique_1_2", "jnt_menique_2_3"
    };

    trajectory_msgs::msg::JointTrajectoryPoint point;

    for (size_t i = 0; i < TOTAL_JOINTS; ++i){
        std_msgs::msg::Float64 msg;

        // 0. Giro de base hacia la lata
        if(i==0){
            msg.data = 0.159; // Teach Pendant: jnt_pecho_hombro
        }
        // 1. Alineación Lateral Constante
        else if(i==1){
            msg.data = 0.0; // Teach Pendant: pose exacta de agarre
        }
        // 2. Extensión del Brazo
        else if(i==4){
            msg.data = -1.068; // Teach Pendant
        }
        else if (i==3) {
            msg.data = 1.5010; // Teach Pendant
        }
        // Orientación de muñeca — palma apunta hacia abajo para contacto limpio
        else if (i==5) {
            msg.data = -1.1309; // Teach Pendant: -64° evita antebrazazo
        }
        // Altura del hombro — codo elevado para aproximación tipo garra
        else if(i==2){
            msg.data = -1.20; // Teach Pendant: codo elevado, apunta parte superior de la lata
        }
        // 4. Agarre al chocar
        else if(colisionDetectada) {
            if(i==6) msg.data = 1.5708; else if(i==7) msg.data = 0.2094; else if(i==8) msg.data = 0.5236;
            else if(i==9) msg.data = 0.80; else if(i==10) msg.data = 0.6109; else if(i==11) msg.data = 0.6981;
            else if(i==12) msg.data = 1.1345; else if(i==13) msg.data = 0.6109; else if(i==14) msg.data = 0.6109;
            else if(i==15) msg.data = 1.1345; else if(i==16) msg.data = 0.6109; else if(i==17) msg.data = 0.6109;
            else if(i==18) msg.data = 0.80; else if(i==19) msg.data = 0.6109; else if(i==20) msg.data = 0.6109;
            else msg.data = 0.0;
        }
        else{
            msg.data = 0.0;
        }
        point.positions.push_back(msg.data);
    }

    point.velocities.assign(TOTAL_JOINTS, 0.0);
    point.accelerations.assign(TOTAL_JOINTS, 0.0);

    if (!colisionDetectada) {
        // Waypoint pre-agarre: brazo vuela alto por encima del pedestal
        trajectory_msgs::msg::JointTrajectoryPoint waypoint = point;
        waypoint.positions[2] = -0.5; // jnt_hombro_biceps elevado — esquiva el pedestal
        waypoint.time_from_start.sec = 1;
        waypoint.time_from_start.nanosec = 500000000; // t = 1.5 s
        jointTrajectoryMsg.points.push_back(waypoint);

        // Pose final: desciende verticalmente sobre la lata
        point.positions[2] = -1.2874; // jnt_hombro_biceps — valor ganador RQT
        point.time_from_start.sec = 3;
        point.time_from_start.nanosec = 0;            // t = 3.0 s
    } else {
        // Fase de agarre: un solo punto, mantener dedos cerrados
        point.time_from_start.sec = 2;
        point.time_from_start.nanosec = 0;
    }

    jointTrajectoryMsg.points.push_back(point);
    jointTrajectoryPub->publish(jointTrajectoryMsg);
}