#include <BetterSerialPlotter/Plot.hpp>
#include <BetterSerialPlotter/PlotMonitor.hpp>
#include <BetterSerialPlotter/BSP.hpp>
#include <Mahi/Gui.hpp>
#include <iostream>
#include <array>

namespace bsp{

Plot::Plot(PlotMonitor* plot_monitor_): 
    plot_monitor(plot_monitor_)
    {}

Plot::Plot(): plot_monitor(nullptr){}

void Plot::make_plot(float time, int plot_num){
    ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 3);
    ImPlot::PushStyleVar(ImPlotStyleVar_LabelPadding,ImVec2(3,2));
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding,ImVec2(2,2));
    ImPlot::PushStyleColor(ImPlotCol_FrameBg,ImVec4(0.33f,0.35f,0.39f,1.0f));
    // set x_axis limits if we aren't paused
    if(!plot_monitor->paused){
        float x_min = 0.0f;
        float x_max = 1.0f;

        std::vector<bool> active_other_x_indices;
        // if we are a user-variable as the x-axis, 
        if (other_x_axis && get_data(x_axis) != std::nullopt){
            // if (get_data(x_axis) != std::nullopt){
                auto other_x_data = get_data(x_axis)->get().get_y();
                active_other_x_indices = std::vector<bool>(other_x_data.size(),false);
                // if it is a realtime x-axis, get lragest data (aka most-recent time)
                if (x_axis_realtime){
                    float most_recent_time = 0.0f;
                    if (get_data(x_axis)->get().Data.size()){
                        most_recent_time = mahi::util::max_element(other_x_data);
                    }
                    x_min = most_recent_time - time_frame;
                    x_max = most_recent_time;
                    ImPlot::SetNextPlotLimitsX(x_min, x_max, ImGuiCond_Always);
                }
                // otherwise, if it is autoscale, get the min and max elements of the data
                else if(autoscale){
                    if (get_data(x_axis)->get().Data.size()){
                        x_min = mahi::util::min_element(other_x_data);
                        x_max = mahi::util::max_element(other_x_data);
                    }
                    ImPlot::SetNextPlotLimitsX(x_min, x_max, ImGuiCond_Always); 
                }
                for (auto i = 0; i < other_x_data.size(); i++){
                    if (other_x_data[i] <= x_max && other_x_data[i] >= x_min){
                        active_other_x_indices[i] = true;    
                    }
                }
            // }
        }
        // otherwise, we just use time with timeframe to set x-lims
        else{
            x_min = time - time_frame;
            x_max = time;
            ImPlot::SetNextPlotLimitsX(x_min, x_max, ImGuiCond_Always);
        }
        // set y_axis limits if autoscaled
        if (autoscale && get_data(y_axis.begin()->first) != std::nullopt){
            // vectors which contain min and max for y axis 0 and y axis 1
            std::vector<float> y_min = {0.0f,0.0f};
            std::vector<float> y_max = {1.0f,1.0f};
            std::vector<bool>  axis_exists = {false,false};

            // go through each of the variables for the plot
            for (auto it = y_axis.begin(); it != y_axis.end(); it++){
                // make sure the variable has data first
                auto data = get_data(it->first);
                if(data != std::nullopt && !data->get().Data.empty()){

                    auto x_vals = get_data(it->first)->get().get_x();
                    auto y_vals = get_data(it->first)->get().get_y();

                    for (auto i = 0; i < x_vals.size(); i++){

                        // check if it is actually visible on the plot
                        if (((x_vals[i] >= x_min && x_vals[i] <= x_max) && !other_x_axis) || 
                            (other_x_axis && active_other_x_indices[i]) ){
                            // if it is first read through, set initial lims to the first value
                            if(!axis_exists[it->second]){
                                y_min[it->second] = y_vals[i];
                                y_max[it->second] = y_vals[i];
                                axis_exists[it->second] = true;
                            }
                            // otherwise, check if these are min and max values
                            else if (y_vals[i] < y_min[it->second]){
                                y_min[it->second] = y_vals[i];
                            }
                            else if (y_vals[i] > y_max[it->second]){
                                y_max[it->second] = y_vals[i];
                            }
                        }
                    }
                }
            }
            // only if we marked axes as existing, set lims based on found values
            for (auto i = 0; i < 2; i++){            
                if (axis_exists[i]){
                    ImPlot::SetNextPlotLimitsY(y_min[i],y_max[i],ImGuiCond_Always,i);
                }
                else if(i == 0){
                    std::cout << name + " axis 0 doesn't exist\n";
                }
            }        
        }
    }
    
    // make plot given x and y variables
    if(ImPlot::BeginPlot(name.c_str(), other_x_axis ? plot_monitor->gui->get_name(x_axis).c_str() : "Time (s)", 0, {-1,plot_height}, ImPlotFlags_NoMenus | ImPlotFlags_YAxis2, 0, 0)){
        
        plot_data(); // see plot_data() function
        
        // make drag and drop targets for all axes and the main plot area
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_PLOT")) {
                int i = *(int*)payload->Data;
                add_identifier(plot_monitor->gui->all_data[i].identifier);
            }
            ImGui::EndDragDropTarget();
        }
        for (int y = 0; y < 2; ++y) {
            if (ImPlot::BeginDragDropTargetY(y)) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_PLOT")) {
                    int i = *(int*)payload->Data;
                    add_identifier(plot_monitor->gui->all_data[i].identifier, y);                    
                }
                ImPlot::EndDragDropTarget();
            }
        }
        if (ImPlot::BeginDragDropTargetX()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_PLOT")) {
                int i = *(int*)payload->Data;
                other_x_axis = true;
                x_axis = plot_monitor->gui->all_data[i].identifier;
            }
            ImPlot::EndDragDropTarget();
        }
        
        // handle x-axis resizing if hovering x-axis
        if(ImPlot::IsPlotXAxisHovered()){
            if (other_x_axis & !x_axis_realtime) paused_x_axis_modifier += plot_monitor->gui->io.MouseWheel/20.0f;
            else time_frame *= 1.0f+plot_monitor->gui->io.MouseWheel/100.0f;
        }
        // handle context menus
        if (ImGui::IsMouseReleased(1)){
            if (ImPlot::IsPlotHovered()){
                ImGui::OpenPopup("##BSPPlotContext");
            }

            else if (ImPlot::IsPlotXAxisHovered()){
                ImGui::OpenPopup("##XAxisContext"); // not yet implemented
            }

            else if (ImPlot::IsPlotYAxisHovered(0)){
                ImGui::OpenPopup("##YAxis0Context"); // not yet implemented
            }

            else if (ImPlot::IsPlotYAxisHovered(1)){
                ImGui::OpenPopup("##YAxis1Context"); // not yet implemented
            }
        }

        // context menu for right clicking main area
        if(ImGui::BeginPopup("##BSPPlotContext")){
            // add data to plot
            if ((ImGui::BeginMenu("Add Data"))){
                for (auto i = 0; i < plot_monitor->gui->all_data.size(); i++){
                    ImPlot::ItemIcon(plot_monitor->gui->get_color(plot_monitor->gui->all_data[i].identifier)); ImGui::SameLine();
                    if(ImGui::BeginMenu(plot_monitor->gui->get_name(plot_monitor->gui->all_data[i].identifier).c_str())){
                        // add based on y axis
                        for (auto y = 0; y < 2; y++){
                            char menu_item_name[10];
                            sprintf(menu_item_name, "Y-Axis %i", y);
                            if(ImGui::MenuItem(menu_item_name)){
                                add_identifier(plot_monitor->gui->all_data[i].identifier,y);
                            }
                        }
                        // add to x axis
                        if(ImGui::MenuItem("X-Axis")){
                            x_axis = plot_monitor->gui->all_data[i].identifier;
                            other_x_axis = true;
                        }
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndMenu();
            } 
            // remove data from plot
            if ((ImGui::BeginMenu("Remove Data"))){
                for (auto i = 0; i < all_plot_data.size(); i++){
                    ImPlot::ItemIcon(plot_monitor->gui->get_color(all_plot_data[i])); ImGui::SameLine();
                    if(ImGui::MenuItem((plot_monitor->gui->get_name(all_plot_data[i]) + " (y-axis " + std::to_string(y_axis[all_plot_data[i]]) + ")").c_str())){
                        remove_identifier(get_data(all_plot_data[i])->get().identifier);
                        break;
                    }
                }
                if (other_x_axis){
                    ImPlot::ItemIcon(plot_monitor->gui->get_color(x_axis));  ImGui::SameLine();
                    if(ImGui::MenuItem((plot_monitor->gui->get_name(x_axis) + " (x-axis)").c_str())){
                        other_x_axis = false;
                        x_axis = -1;
                    }
                }
                ImGui::EndMenu();
            } 
            ImGui::MenuItem("Auto Scale",0,&autoscale);
            ImGui::MenuItem("Realtime X-axis",0,&x_axis_realtime);
            ImGui::EndPopup();
        }

        // update and region of the plot for dragging to resize        
        plot_y_end = ImGui::GetWindowPos().y + ImGui::GetWindowContentRegionMax().y;
        ImPlot::EndPlot();
    }
    
    // reize plot if we are in the right area
    if (ImGui::GetMousePos().y <= plot_y_end + resize_area && ImGui::GetMousePos().y >= plot_y_end - resize_area){
        ImGui::SetMouseCursor(3);
        if (ImGui::IsMouseClicked(0)){
            is_resizing = true;
        }
    }

    if (ImGui::IsMouseReleased(0) && is_resizing){
        is_resizing = false;
        ImGui::SetMouseCursor(0);
    }

    if (is_resizing){
        plot_height += plot_monitor->gui->io.MouseDelta.y;
        ImGui::SetMouseCursor(3);
    }
    ImPlot::PopStyleColor();
    ImPlot::PopStyleVar();
    ImPlot::PopStyleVar();
    ImPlot::PopStyleVar();
    // std::cout << "end plot\n";
}

void Plot::plot_data(){

    for (auto i = 0; i < all_plot_data.size(); i++){
        if (get_data(all_plot_data[i]) == std::nullopt) break;
        // get the correct set of data based on what we are currently doing
        auto &curr_data = plot_monitor->paused ? all_plot_paused_data[i] : get_data(all_plot_data[i])->get();
        auto &curr_identifier = plot_monitor->paused ? all_plot_paused_data[i].identifier : all_plot_data[i];
        float* curr_x_data; // would prefer this to be a reference, but have to assign immediately, so can't
        if (plot_monitor->paused){
            curr_x_data = (other_x_axis) ? &paused_x_axis.Data[0].y : &all_plot_paused_data[i].Data[0].x;
        }
        else{
            curr_x_data = (other_x_axis) ? &get_data(x_axis)->get().Data[0].y : &curr_data.Data[0].x;
        }

        ImPlot::PushStyleColor(ImPlotCol_Line,plot_monitor->gui->get_color(curr_data.identifier)); 
        char id[64];
        sprintf(id,"%s###%i",plot_monitor->gui->get_name(curr_data.identifier).c_str(),i);
        ImPlot::SetPlotYAxis(y_axis[curr_identifier]);
        ImPlot::PlotLine(id, curr_x_data, &curr_data.Data[0].y, curr_data.Data.size(), curr_data.Offset, 2 * sizeof(float));  
        ImPlot::PopStyleColor();   
    }   
    
}

void Plot::add_identifier(char identifier, int y_axis_num){
    bool exists = false;
    // check if it is already there
    for (const auto &i : all_plot_data){
        if (identifier == i) {
            exists = true;
            break;
        }
    }
    
    // if it isnt already there, add it and set y axis to 0 (default)
    if (!exists) all_plot_data.push_back(identifier);
    
    y_axis[identifier] = y_axis_num;
}

void Plot::remove_identifier(char identifier){
    // look for the identifier in the identifiers vector
    for (auto i = all_plot_data.begin(); i != all_plot_data.end(); i++){
        if (*i == identifier) {
            all_plot_data.erase(i);
            break;
        }
    }
    
    // look for the identifier in the y_axis unordered map
    for (auto i = y_axis.begin(); i != y_axis.end(); i++){
        if (i->first == identifier) {
            y_axis.erase(i);
            break;
        }
    }
}

bool Plot::has_identifier(char identifier) const{
    for (const auto &data : all_plot_data){
        if (data == identifier) return true;
    }
    return false;
}

std::optional<std::reference_wrapper<ScrollingData>> Plot::get_data(char identifier){
    return plot_monitor->gui->get_data(identifier);
}

void Plot::update_paused_data(){
    all_plot_paused_data.clear();
    all_plot_paused_data.reserve(all_plot_data.size());
    for (const auto& data : all_plot_data){
        all_plot_paused_data.push_back(get_data(data)->get());
    }
    if (other_x_axis) paused_x_axis = get_data(x_axis)->get();
}

}