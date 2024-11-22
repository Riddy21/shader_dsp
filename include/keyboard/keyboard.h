#pragma once
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <unordered_map>
#include <unordered_set>

#include "keyboard/key.h"
#include "audio_render_stage/audio_generator_render_stage.h"

/**
 * @class Keyboard
 * @brief Singleton class that manages keyboard input and key states.
 *
 * The Keyboard class is responsible for handling keyboard input, managing key states,
 * and interfacing with the audio renderer to produce sound based on key presses.
 *
 * @note This class follows the singleton design pattern.
 */
class Keyboard {
public:
    /**
     * @brief Get the singleton instance of the Keyboard class.
     * 
     * This method returns the single instance of the Keyboard class, creating it if it does not already exist.
     * 
     * @return Keyboard& Reference to the singleton instance of the Keyboard class.
     */
    static Keyboard & get_instance() {
        if (!instance) {
            instance = new Keyboard();
        }
        return *instance;
    }

    /**
     * @brief Deleted copy constructor.
     * 
     * Prevents copying of the Keyboard instance.
     * 
     * @param other Another instance of Keyboard (unused).
     */
    Keyboard(Keyboard const&) = delete;

    /**
     * @brief Deleted assignment operator.
     * 
     * Prevents assignment of the Keyboard instance.
     * 
     * @param other Another instance of Keyboard (unused).
     * @return Keyboard& Reference to the current instance.
     */
    void operator=(Keyboard const&) = delete;

    /**
     * @brief Initialize the keyboard.
     * 
     * This method initializes the keyboard, setting up necessary states and configurations.
     * 
     * @return bool True if initialization is successful, false otherwise.
     */
    bool initialize();

    /**
     * @brief Add a key to the keyboard.
     * 
     * This method adds a key to the keyboard's internal key map.
     * 
     * @param key Pointer to the Key object to be added.
     */
    void add_key(Key * key);

    /**
     * @brief Get a key from the keyboard.
     * 
     * This method retrieves a key from the keyboard's internal key map based on the given key code.
     * 
     * @param key The key code of the key to retrieve.
     * @return Key* Pointer to the Key object corresponding to the given key code.
     */
    Key * get_key(const unsigned char key) { return m_keys[key].get(); }

private:
    /**
     * @brief Constructor for the Keyboard class.
     * 
     * This constructor is private to enforce the singleton pattern.
     */
    Keyboard() {}

    /**
     * @brief Destructor for the Keyboard class.
     * 
     * Cleans up resources used by the Keyboard class.
     */
    ~Keyboard();

    /**
     * @brief Callback function for key down events.
     * 
     * This static method is called when a key is pressed down.
     * 
     * @param key The key code of the key that was pressed.
     * @param x The x-coordinate of the mouse position when the key was pressed.
     * @param y The y-coordinate of the mouse position when the key was pressed.
     */
    static void key_down_callback(unsigned char key, int x, int y);

    /**
     * @brief Callback function for key up events.
     * 
     * This static method is called when a key is released.
     * 
     * @param key The key code of the key that was released.
     * @param x The x-coordinate of the mouse position when the key was released.
     * @param y The y-coordinate of the mouse position when the key was released.
     */
    static void key_up_callback(unsigned char key, int x, int y);

    static Keyboard * instance;

    unsigned int m_num_octaves;
    AudioRenderer & m_audio_renderer = AudioRenderer::get_instance();
    std::unordered_map<unsigned char, std::unique_ptr<Key>> m_keys;
};

#endif // KEYBOARD_H