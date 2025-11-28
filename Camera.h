#ifndef CAMERA_H
#define CAMERA_H

#include <GLFW/glfw3.h>
#include "matLibrary.h"

// Clase para manejar la cámara libre
class Camera {
public:
    Vec3 position;
    Vec3 front;
    Vec3 up;
    Vec3 right;
    Vec3 worldUp;

    float yaw;
    float pitch;
    float movementSpeed;
    float mouseSensitivity;
    float zoom;

    Camera(Vec3 startPos = Vec3(0.0f, 0.0f, 6.0f), Vec3 startUp = Vec3(0.0f, 1.0f, 0.0f)) 
        : front(Vec3(0.0f, 0.0f, -1.0f)), movementSpeed(2.5f), mouseSensitivity(0.1f), zoom(45.0f) {
        position = startPos;
        worldUp = startUp;
        yaw = -90.0f;
        pitch = 0.0f;
        updateCameraVectors();
    }

    Mat4 getViewMatrix() {
        return lookAt(position, position + front, up);
    }

    void processKeyboard(int direction, float deltaTime) {
        float velocity = movementSpeed * deltaTime;
        if (direction == 0) // FORWARD
            position = position + (front * velocity);
        if (direction == 1) // BACKWARD
            position = position - (front * velocity);
        if (direction == 2) // LEFT
            position = position - (right * velocity);
        if (direction == 3) // RIGHT
            position = position + (right * velocity);
        if (direction == 4) // UP
             position = position + (worldUp * velocity);
        if (direction == 5) // DOWN
             position = position - (worldUp * velocity);
    }

    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw += xoffset;
        pitch += yoffset;

        if (constrainPitch) {
            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
        }
        updateCameraVectors();
    }

private:
    void updateCameraVectors() {
        Vec3 newFront;
        newFront.x = cos(yaw * M_PI / 180.0f) * cos(pitch * M_PI / 180.0f);
        newFront.y = sin(pitch * M_PI / 180.0f);
        newFront.z = sin(yaw * M_PI / 180.0f) * cos(pitch * M_PI / 180.0f);
        front = normalize(newFront);
        right = normalize(cross(front, worldUp));
        up = normalize(cross(right, front));
    }
};

#endif