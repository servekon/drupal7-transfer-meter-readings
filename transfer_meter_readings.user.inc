<?php
/**
 * Page of view meters
 * */
function transfer_meter_readings_form(){
	$form = drupal_get_form('transfer_meter_readings_send_readings_form');
    return drupal_render($form);
}

/**
 * List steps for the payment form
 * */
function _transfer_meter_readings_form_steps(){
    return array(
        1 => array(
            'form' => 'transfer_meter_readings_step_one_form',
        ),
        2 => array(
            'form' => 'transfer_meter_readings_step_two_form',
        ),
    );
}

/**
 * Multi-step form to send meter readings
 * */
function transfer_meter_readings_send_readings_form($form, &$form_state){
    if(empty($form_state['step'])){
        $form_state['step']=1;
        $form_state['step_forms'] = _transfer_meter_readings_form_steps();
    }
	$step = &$form_state['step'];
	
	$form = $form_state['step_forms'][$step]['form']($form, $form_state);
	
    if ($step > 1){
        $form['prev'] = array(
            '#type' => 'submit',
            '#value' => t('Prev'),
            '#name' => 'prev',
            '#submit' => array('_tmr_send_readings_form_previous_submit'),
            '#limit_validation_errors' => array(),
            '#weight' => 20,
        );
    }
    
    if ($step < sizeof($form_state['step_forms'])){
        $form['next'] = array(
            '#type' => 'submit',
            '#value' => t('Next'),
            '#name' => 'next',
            '#submit' => array('_tmr_send_readings_form_next_submit'),
            '#weight' => 21,
        );
    }
    
    if (function_exists($form_state['step_forms'][$step]['form'] . '_validate')){
        $form['next']['#validate'] = array($form_state['step_forms'][$step]['form'].'_validate');
    }
    $form['#action'] = url('guest-meter');
    return $form;
}

/*
 * Go to back previos step
 * */
function _tmr_send_readings_form_previous_submit($form, &$form_state){
    $step = &$form_state['step'];
    $form_state['step_forms'][$step]['stored_values'] = $form_state['values'];
    if ($step > 1) {
        $step--;
        $form_state['values'] = $form_state['step_forms'][$step]['stored_values'];
    }
    $form_state['rebuild'] = TRUE;
}

/**
 * Next step submit
 * */
function _tmr_send_readings_form_next_submit($form, &$form_state){
    $step = &$form_state['step'];
    $form_state['step_forms'][$step]['stored_values'] = $form_state['values'];
    if ($step < sizeof($form_state['step_forms'])){
        $step++;
        if (!empty($form_state['step_forms'][$step]['stored_values'])){
            $form_state['values'] = $form_state['step_forms'][$step]['stored_values'];
        }
        else {
            $form_state['values'] = array();
        }
        $form_state['rebuild'] = TRUE;
        return;
    }
}

/*
 * Validate step one form
 * */
function transfer_meter_readings_step_one_form_validate($form, &$form_state){
    $check 	    = array();
	$street 	= _tmr_isset_var($form_state['values']['tmr_street']);
	$house      = _tmr_isset_var($form_state['values']['tmr_house']);
	$flat	    = _tmr_isset_var($form_state['values']['flat_num']);
	$account	= _tmr_isset_var($form_state['values']['account']);
    
    $check = db_select('tmr_main_data', 'd')
    ->fields('d', array('md_id'))
    ->condition('d.md_st_key', (int)$street)
    ->condition('d.md_house_key', (int)$house)
    ->condition('d.md_flat', check_plain(trim($flat)))
    ->condition('d.md_account', check_plain(trim($account)))
    ->range(0,1)
    ->execute()->fetchAssoc();
    if(empty($check)){
        form_set_error('account', t("Specified account does not exist."));
    }
}

/*
 * Step one form
 * */
function transfer_meter_readings_step_one_form($form, &$form_state){
    try{
        $arrStreet = array();
        
        $qStreet = db_select('tmr_handbook_housing_stock', 'hs')
        ->fields('hs', array('hhs_st_key', 'hhs_st_name'))
        ->orderBy('hhs_st_name', 'ASC')
        ->execute()
        ->fetchAllAssoc('hhs_st_key');
        
        foreach($qStreet as $key=>$val){
            $arrStreet[$val->hhs_st_key] = $val->hhs_st_name;
        }
        
        $form['tmr_street'] = array(
            '#type' => 'select',
            '#title' => t('Street'),
            '#required' => TRUE,
            '#options' => $arrStreet,
            '#ajax' => array(
              'callback' => 'gethouse_callback',
              'wrapper' => 'rmr-house-wrapper',
              'event' => 'change',
              'method' => 'replace',
            ),
            '#default_value' => !empty($form_state['values']['tmr_street']) ? $form_state['values']['tmr_street'] : NULL,
        );
        $selected = isset($form_state['values']['tmr_street']) ? $form_state['values']['tmr_street'] : 0;
        $form['tmr_house'] = array(
            '#type' => 'select',
            '#title' => t('Number of house'),
            '#prefix' => '<div id="rmr-house-wrapper">', '#suffix' => '</div>',
            '#required' => TRUE,
            '#options' => _tmr_get_house($selected),
            '#default_value' => !empty($form_state['values']['tmr_house']) ? $form_state['values']['tmr_house'] : NULL,
        );
        
        $form['flat_num'] = array(
            '#type' => 'textfield',
            '#title' => t('Apartment number'),
            '#required' => TRUE,
            '#default_value' => !empty($form_state['values']['flat_num']) ? $form_state['values']['flat_num'] : '',
            #size' => '10'
        );
        
        $form['account'] = array(
            '#type' => 'textfield',
            '#title' => t('Personal account number'),
            '#required' => TRUE,
            '#default_value' => !empty($form_state['values']['account']) ? $form_state['values']['account'] : '',
            #size' => '10'
        );
        
        $form['redirect'] = FALSE;
        return $form;
    }
    catch(Exception $e){
        watchdog_exception('rmr-error', $e);
        drupal_set_message(t("An error occurred."), 'error', FALSE);
    }
}
/*
 * Step two form
 * */
function transfer_meter_readings_step_two_form($form=array(), &$form_state){
    $prevStep = $form_state['step']-1;
    $form_state['values'] 	= array_merge($form_state['values'],$form_state['step_forms'][$prevStep]['stored_values']);
    $account = $form_state['values']['account'];
    
    try{
        $query = db_select('tmr_main_data', 'md');
        $query->leftJoin('tmr_new_readings', 'nr', 'md.md_account=nr.nr_account and md.md_met_num = nr.nr_met_num');
        $query->fields('md', array('md_id', 'md_order_id', 'md_month', 'md_account', 'md_st_key', 'md_house_key', 'md_flat', 'md_met_num', 'md_met_name', 'md_met_type', 'md_met_val'))
        ->fields('nr', array('nr_met_new_val', 'nr_block'))
        ->condition('md.md_account', $account)
        ->orderBy('md.md_met_num', 'ASC')
        ->groupBy('md_met_num');
        
        $result = $query->execute()->fetchAllAssoc('md_id');
        
        $count = sizeof($result);
        
        $header = array(t('Personal account number'), t('Meter number'), t('Counter Name'), t('Counter type'), t('Previous reading'), t('New indications'));
        $rows = array();
        $i=0;
        foreach($result as $key=>$val) {
            $arr = (array)$val;
            $form['rows'][$key] = array(
                '#prefix' => '<table cellpadding="0" width="98%" cellspacing="0" style="margin:0"><tr>',
                '#suffix' => '</tr></table>',
            );
            if($i == 0){
                $form['rows'][$key]['#prefix'] .= '<th>'.implode('</th><th>', $header).'</th></tr><tr>';
            }
            foreach($arr as $tCol=>$vCol){
                
                $newVal = (isset($form_state['input']['nr_met_new_val'][$key])) ? check_plain($form_state['input']['nr_met_new_val'][$key]) : check_plain($vCol);
                switch($tCol){
                    case 'md_met_num':
                    case 'md_met_name':
                    case 'md_met_type':
                    case 'md_account':
                    case 'md_met_val':
                        $form['rows'][$key][$tCol.'['.$key.']']['#type'] = 'item';
                        $form['rows'][$key][$tCol.'['.$key.']']['#markup'] = $vCol;
                        $form['rows'][$key][$tCol.'['.$key.']']['#prefix'] = '<td width="16%">';
                        $form['rows'][$key][$tCol.'['.$key.']']['#suffix'] = '</td>';
                    break;
                    case 'md_id':
                    case 'md_order_id':
                    case 'nr_block':
                    case 'md_month':
                        $form['rows'][$tCol.'['.$key.']']['#type'] = 'hidden';
                        $form['rows'][$tCol.'['.$key.']']['#value'] = $vCol;
                    break;
                    case 'nr_met_new_val':
                        $form['rows'][$key][$tCol.'['.$key.']']['#type'] = 'textfield';
                        $form['rows'][$key][$tCol.'['.$key.']']['#default_value'] = $newVal;
                        $form['rows'][$key][$tCol.'['.$key.']']['#prefix'] = '<td width="16%">';
                        $form['rows'][$key][$tCol.'['.$key.']']['#suffix'] = '</td>';
                        $form['rows'][$key][$tCol.'['.$key.']']['#size'] = 10;
                        if(variable_get('tmr_is_ceil_meter_readings', 0) > 0){
                            $form['rows'][$key][$tCol.'['.$key.']']['#default_value'] = ceil($newVal);
                        }
                        if($val->nr_block > 0){
                            $form['rows'][$key][$tCol.'['.$key.']']['#type'] = 'item';
                            $form['rows'][$key][$tCol.'['.$key.']']['#title'] = $vCol;
                            $count--;
                        }
                    break;
                }
                //~ Hidden fields needed for the validation
                if($tCol == 'md_met_val' or $tCol == 'md_account' or $tCol == 'md_met_num'){
                    $form['rows'][$tCol.'['.$key.']']['#type'] = 'hidden';
                    $form['rows'][$tCol.'['.$key.']']['#value'] = $vCol;
                }
            }
            $i++;
        }
        if(!empty($count)){
            $form['submit'] = array(
                '#type' => 'submit',
                '#value' => t('Make statements'),
                '#weight' => 30,
                '#attributes' => array('style'=>array('margin-top:14px;')),
            );
        }
        else{
            drupal_set_message(t('All previously entered readings have been transferred to the processing.'), 'warning');
        }
        return $form;
    }
    catch(Exception $e){
        watchdog_exception('rmr-error', $e);
        drupal_set_message(t("An error occurred."), 'error', FALSE);
    }
}
/*
 * Step two form validate
 * */
function transfer_meter_readings_send_readings_form_validate($form, &$form_state) {
    $isCheckVol = (int)variable_get('tmr_delta_meter_readings', 0);
    if(!isset($form_state['values']['submit']) or !isset($form_state['input']['nr_met_new_val'])){
        form_error($form, t("Meter readings were not included."));
    }
    foreach($form_state['input']['nr_met_new_val'] as $key=>$val){
        if(!empty($val)){
            if (preg_match("/[^.\d]/i", $val)) {
                form_set_error('nr_met_new_val['.$key.']', t("Field %field can be only digits and dot.", array("%field"=>t("''New indications''"))));
            }
            if($val < $form_state['values']['md_met_val['.$key.']']){
                form_set_error('nr_met_new_val['.$key.']', t("Current counter value is less than the previous."));
            }
            if($isCheckVol > 0){
                if(($val-$form_state['values']['md_met_val['.$key.']']) > $isCheckVol){
                    form_set_error('nr_met_new_val['.$key.']', t("The difference between the previous and the current counter value is too high."));
                }
            }
        }
    }
}
/*
 * Step two form submit
 * */
function transfer_meter_readings_send_readings_form_submit($form, &$form_state){
    $step = &$form_state['step'];
    $form_state['step_forms'][$step]['stored_values'] = $form_state['values'];
    $isFloat = variable_get('tmr_is_ceil_meter_readings', 0);
    $count = sizeof($form_state['input']['nr_met_new_val']);
    foreach($form_state['input']['nr_met_new_val'] as $key=>$val){
        if($form_state['values']['nr_block['.$key.']'] > 0){
            $count--;
        }
        if(!empty($val) and $form_state['values']['nr_block['.$key.']'] == 0){
            if($isFloat > 0){
				$val = ceil($val);
			}
            try{
				$month = check_plain($form_state['values']['md_month['.$key.']']);
				$account = check_plain($form_state['values']['md_account['.$key.']']);
                $metNum = (int)$form_state['values']['md_met_num['.$key.']'];
                $metVal = (float)$form_state['values']['md_met_val['.$key.']'];
                //~ Remove meter readings
                db_delete('tmr_new_readings')->condition('nr_account', $account)
                ->condition('nr_met_num', $metNum)
                ->condition('nr_met_val', $metVal)
                ->where('YEAR(nr_month) = :year AND MONTH(nr_month) = :month', array(':year'=>date('Y', strtotime($month)), ':month'=>date('n', strtotime($month))))
                ->execute();
                //~ Add new meter readings
                $insSel = db_select('tmr_main_data', 'md')
                ->fields('md', array('md_order_id', 'md_month', 'md_account',
                                    'md_st_key', 'md_house_key', 'md_flat', 
                                    'md_met_num', 'md_met_name', 'md_met_type', 'md_met_val'))
                ->condition('md_account', $account)
                ->condition('md_month', $month)
                ->condition('md_met_num', $metNum)
                ->condition('md_met_val', $metVal);
                $insSel->addExpression($val, 'new_val');
                $insSel->addExpression(1, 'block');
                $insSel->addExpression(REQUEST_TIME, 'date');
                $insSel->addExpression(REQUEST_TIME, 'date_change');
                
                $nr = db_insert('tmr_new_readings')
                ->fields(array('nr_order_id', 'nr_month', 'nr_account', 
                                'nr_st_key', 'nr_house_key', 'nr_flat',
                                'nr_met_num', 'nr_met_name', 'nr_met_type', 'nr_met_val',
                                'nr_met_new_val', 'nr_block', 'nr_ins_date', 'date_change'))
                ->from($insSel)->execute();
                
                drupal_set_message(t("Meter readings were made."), 'status', FALSE);
			}
			catch(Exception $e){
				watchdog_exception('rmr-error', $e);
				drupal_set_message(t("Meter readings were not included.").t('More information in '.l('syslog','admin/reports/dblog').'.').'<br />'.$e->getMessage(), 'error', FALSE); return false;
			}
        }
    }
    if($count > 0){
        $form_state['rebuild'] = TRUE;
        return;
    }
}
/*
 * Ajax callback for get list of houses by street key
 * */
function gethouse_callback($form, $form_state){
	$form['tmr_house']['#options'] = _tmr_get_house($form_state['input']['tmr_street']);
	return $form['tmr_house'];
}
/*
 * Get list of houses by street key
 * */
function _tmr_get_house($street){
	$arrDom=array();
	if(!empty($street)){
		$houses = db_select('tmr_handbook_housing_stock', 'hs')
        ->fields('hs', array('hhs_house_key', 'hhs_hs_house'))
        ->condition('hhs_st_key', (int)$street)
        ->orderBy('hs.hhs_hs_house', 'ASC')
        ->execute()
        ->fetchAllAssoc('hhs_house_key');
        
        if ($houses == false OR sizeof($houses) < 0) {
			drupal_set_message(t("No data."), 'error', FALSE);
		}
		
		foreach($houses as $key=>$val){
			$arrDom[$val->hhs_house_key]=$val->hhs_hs_house;
		}
		unset($key, $val);
	}
	return $arrDom;
}
