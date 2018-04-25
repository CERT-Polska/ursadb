#pragma once

#include <iostream>
#include <iomanip>

class ProgressBar {
    std::string job;
    int cur_work;
    int max_work;
    int max_digits;
    int bar_width;

    void set_job(const std::string &new_job) {
        job = new_job.substr(0, 10);
        bar_width = 70 - (max_digits*2) - (unsigned int)new_job.length();
    }

public:
    explicit ProgressBar(int max_work, const std::string &job)
            : cur_work(0), max_work(max_work), max_digits(0), bar_width(0), job(job) {
        while (max_work > 0) {
            max_digits += 1;
            max_work /= 10;
        }

        bar_width = 70 - (max_digits*2);
    }

    void update(const std::string &new_job) {
        set_job(new_job);
        update();
    }

    void update(int cur_work) {
        this->cur_work = cur_work;
        std::cout << "\r" << job << " [";

        double perc = (double)cur_work * bar_width / max_work;

        for (int i = 0; i < bar_width; i++) {
            if (perc > i) {
                std::cout << "#";
            } else {
                std::cout << " ";
            }
        }

        std::cout << "] " << std::setw(max_digits) << cur_work << "/" << max_work;
        std::cout.flush();
    }

    void update(int cur_work, const std::string &new_job) {
        set_job(new_job);
        update(cur_work);
    }

    void update() { update(cur_work); }
};
