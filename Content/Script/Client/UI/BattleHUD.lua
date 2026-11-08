local M = UnLua.Class()

local function clamp(value, minimum, maximum)
    return math.max(minimum, math.min(maximum, value))
end

function M:BP_StartHUDFlow(preview_enabled)
    self.PreviewEnabled = preview_enabled
    self.PreviewState = 0
    self.PreviewElapsed = 0.0
    self.FormalHealth = nil
    self.FormalMaxHealth = nil
    self.FormalMagazineAmmo = nil
    self.FormalReserveAmmo = nil
    self.FormalReloading = false
    self.FormalReloadProgress = 0.0

    if self.PreviewEnabled then
        self:ApplyPreviewFrame()
    end
end

function M:BP_StopHUDFlow()
    self.PreviewEnabled = false
end

function M:BP_ReceiveHealthState(current_health, max_health)
    self.FormalHealth = current_health
    self.FormalMaxHealth = max_health

    if not self.PreviewEnabled then
        self:SetHealthDisplay(current_health, max_health)
    end
end

function M:BP_ReceiveAmmoState(magazine_ammo, reserve_ammo)
    self.FormalMagazineAmmo = magazine_ammo
    self.FormalReserveAmmo = reserve_ammo

    if not self.PreviewEnabled then
        self:SetAmmoDisplay(magazine_ammo, reserve_ammo)
    end
end

function M:BP_ReceiveReloadState(is_reloading, normalized_progress)
    self.FormalReloading = is_reloading
    self.FormalReloadProgress = normalized_progress

    if not self.PreviewEnabled then
        self:SetReloadDisplay(is_reloading, normalized_progress)
    end
end

function M:BP_SetDevelopmentPreviewEnabled(enabled)
    self.PreviewEnabled = enabled
    self.PreviewState = self.PreviewState or 0
    self.PreviewElapsed = 0.0

    if enabled then
        self:ApplyPreviewFrame()
    else
        self:RestoreFormalState()
    end
end

function M:BP_SetDevelopmentPreviewState(state_index)
    self.PreviewState = clamp(state_index, 0, 4)
    self.PreviewElapsed = 0.0

    if self.PreviewEnabled then
        self:ApplyPreviewFrame()
    end
end

function M:BP_TickHUDFlow(delta_seconds)
    if not self.PreviewEnabled then
        return
    end

    self.PreviewElapsed = self.PreviewElapsed + delta_seconds
    self:ApplyPreviewFrame()
end

function M:RestoreFormalState()
    if self.FormalHealth ~= nil and self.FormalMaxHealth ~= nil then
        self:SetHealthDisplay(self.FormalHealth, self.FormalMaxHealth)
    end

    if self.FormalMagazineAmmo ~= nil and self.FormalReserveAmmo ~= nil then
        self:SetAmmoDisplay(self.FormalMagazineAmmo, self.FormalReserveAmmo)
    end

    self:SetReloadDisplay(self.FormalReloading, self.FormalReloadProgress)
end

function M:ApplyPreviewFrame()
    local active_state = self.PreviewState
    if active_state == 0 then
        local cycle_time = math.fmod(self.PreviewElapsed, 12.0)
        if cycle_time < 3.0 then
            active_state = 1
        elseif cycle_time < 6.0 then
            active_state = 2
        elseif cycle_time < 8.0 then
            active_state = 3
        else
            active_state = 4
        end
    end

    if active_state == 1 then
        self:SetHealthDisplay(100.0, 100.0)
        self:SetAmmoDisplay(30, 90)
        self:SetReloadDisplay(false, 0.0)
    elseif active_state == 2 then
        self:SetHealthDisplay(42.0, 100.0)
        self:SetAmmoDisplay(24, 72)
        self:SetReloadDisplay(false, 0.0)
    elseif active_state == 3 then
        self:SetHealthDisplay(42.0, 100.0)
        self:SetAmmoDisplay(6, 54)
        self:SetReloadDisplay(false, 0.0)
    else
        local reload_progress = math.fmod(self.PreviewElapsed, 2.0) / 2.0
        self:SetHealthDisplay(42.0, 100.0)
        self:SetAmmoDisplay(6, 54)
        self:SetReloadDisplay(true, reload_progress)
    end
end

return M
