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
    self.FormalDead = false
    self.FormalRespawnSeconds = 0.0
    self.FormalProtected = false
    self.FormalProtectionSeconds = 0.0
    self.HitSeconds = 0.0
    self.DamageSeconds = 0.0

    self:SetHitDisplay(false)
    self:SetDamageDisplay(0.0)
    self:SetDeathDisplay(false, 0.0)
    self:SetProtectionDisplay(false, 0.0)

    if self.PreviewEnabled then
        self:ApplyPreviewFrame()
    end
end

function M:BP_StopHUDFlow()
    self.PreviewEnabled = false
    self.HitSeconds = 0.0
    self.DamageSeconds = 0.0
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

function M:BP_ReceiveHitConfirmed()
    if self.PreviewEnabled then
        return
    end

    self.HitSeconds = 0.35
    self:SetHitDisplay(true)
end

function M:BP_ReceiveDamageFeedback()
    if self.PreviewEnabled then
        return
    end

    self.DamageSeconds = 0.6
    self:SetDamageDisplay(0.22)
end

function M:BP_ReceiveDeathState(is_dead, remaining_seconds)
    self.FormalDead = is_dead
    self.FormalRespawnSeconds = math.max(0.0, remaining_seconds)

    if not self.PreviewEnabled then
        self:SetDeathDisplay(is_dead, self.FormalRespawnSeconds)
    end
end

function M:BP_ReceiveProtectionState(is_protected, remaining_seconds)
    self.FormalProtected = is_protected
    self.FormalProtectionSeconds = math.max(0.0, remaining_seconds)

    if not self.PreviewEnabled then
        self:SetProtectionDisplay(is_protected, self.FormalProtectionSeconds)
    end
end

function M:BP_ResetCombatFeedback()
    self.FormalDead = false
    self.FormalRespawnSeconds = 0.0
    self.FormalProtected = false
    self.FormalProtectionSeconds = 0.0
    self.HitSeconds = 0.0
    self.DamageSeconds = 0.0

    if not self.PreviewEnabled then
        self:SetHitDisplay(false)
        self:SetDamageDisplay(0.0)
        self:SetDeathDisplay(false, 0.0)
        self:SetProtectionDisplay(false, 0.0)
    end
end

function M:BP_SetDevelopmentPreviewEnabled(enabled)
    self.PreviewEnabled = enabled
    self.PreviewState = self.PreviewState or 0
    self.PreviewElapsed = 0.0
    self.HitSeconds = 0.0
    self.DamageSeconds = 0.0

    if enabled then
        self:ApplyPreviewFrame()
    else
        self:RestoreFormalState()
    end
end

function M:BP_SetDevelopmentPreviewState(state_index)
    self.PreviewState = clamp(state_index, 0, 9)
    self.PreviewElapsed = 0.0

    if self.PreviewEnabled then
        self:ApplyPreviewFrame()
    end
end

function M:BP_TickHUDFlow(delta_seconds)
    if self.PreviewEnabled then
        self.PreviewElapsed = self.PreviewElapsed + math.max(0.0, delta_seconds)
        self:ApplyPreviewFrame()
        return
    end

    if self.HitSeconds > 0.0 then
        self.HitSeconds = math.max(0.0, self.HitSeconds - delta_seconds)
        self:SetHitDisplay(self.HitSeconds > 0.0)
    end

    if self.DamageSeconds > 0.0 then
        self.DamageSeconds = math.max(0.0, self.DamageSeconds - delta_seconds)
        self:SetDamageDisplay(0.22 * self.DamageSeconds / 0.6)
    end
end

function M:RestoreFormalState()
    if self.FormalHealth ~= nil and self.FormalMaxHealth ~= nil then
        self:SetHealthDisplay(self.FormalHealth, self.FormalMaxHealth)
    end

    if self.FormalMagazineAmmo ~= nil and self.FormalReserveAmmo ~= nil then
        self:SetAmmoDisplay(self.FormalMagazineAmmo, self.FormalReserveAmmo)
    end

    self:SetReloadDisplay(self.FormalReloading, self.FormalReloadProgress)
    self:SetHitDisplay(false)
    self:SetDamageDisplay(0.0)
    self:SetDeathDisplay(self.FormalDead, self.FormalRespawnSeconds)
    self:SetProtectionDisplay(self.FormalProtected, self.FormalProtectionSeconds)
end

function M:ApplyPreviewFrame()
    local active_state = self.PreviewState
    if active_state == 0 then
        local cycle_time = math.fmod(self.PreviewElapsed, 24.0)
        if cycle_time < 3.0 then
            active_state = 1
        elseif cycle_time < 6.0 then
            active_state = 2
        elseif cycle_time < 8.0 then
            active_state = 3
        elseif cycle_time < 12.0 then
            active_state = 4
        elseif cycle_time < 15.0 then
            active_state = 5
        elseif cycle_time < 17.0 then
            active_state = 6
        elseif cycle_time < 20.0 then
            active_state = 7
        elseif cycle_time < 22.0 then
            active_state = 8
        else
            active_state = 9
        end
    end

    self:SetHitDisplay(false)
    self:SetDamageDisplay(0.0)
    self:SetDeathDisplay(false, 0.0)
    self:SetProtectionDisplay(false, 0.0)
    self:SetReloadDisplay(false, 0.0)

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
    elseif active_state == 4 then
        local reload_progress = math.fmod(self.PreviewElapsed, 2.0) / 2.0
        self:SetHealthDisplay(42.0, 100.0)
        self:SetAmmoDisplay(6, 54)
        self:SetReloadDisplay(true, reload_progress)
    elseif active_state == 5 then
        self:SetHealthDisplay(42.0, 100.0)
        self:SetAmmoDisplay(24, 72)
        self:SetDamageDisplay(0.22)
    elseif active_state == 6 then
        self:SetHealthDisplay(100.0, 100.0)
        self:SetAmmoDisplay(24, 72)
        self:SetHitDisplay(true)
    elseif active_state == 7 then
        local elapsed = self.PreviewState == 0 and math.fmod(self.PreviewElapsed, 24.0) - 17.0 or self.PreviewElapsed
        self:SetHealthDisplay(0.0, 100.0)
        self:SetAmmoDisplay(0, 0)
        self:SetDeathDisplay(true, math.max(0.0, 3.0 - elapsed))
    elseif active_state == 8 then
        local elapsed = self.PreviewState == 0 and math.fmod(self.PreviewElapsed, 24.0) - 20.0 or self.PreviewElapsed
        self:SetHealthDisplay(100.0, 100.0)
        self:SetAmmoDisplay(30, 90)
        self:SetProtectionDisplay(true, math.max(0.0, 1.5 - elapsed))
    else
        self:SetHealthDisplay(100.0, 100.0)
        self:SetAmmoDisplay(30, 90)
    end
end

return M
